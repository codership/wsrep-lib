/*
 * Copyright (C) 2021 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "wsrep/reporter.hpp"
#include "wsrep/logger.hpp"

#include <sstream>
#include <iomanip>

#include <cstring>  // strerror()
#include <cstdlib>  // mkstemp()
#include <cerrno>   // errno
#include <unistd.h> // write()
#include <cstdio>   // rename(), snprintf()
#include <ctime>    // clock_gettime()
#include <cmath>    // floor()

static std::string const TEMP_EXTENSION(".XXXXXX");

static inline double
timestamp()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return (double(time.tv_sec) + double(time.tv_nsec)*1.0e-9);
}

wsrep::reporter::reporter(wsrep::mutex&      mutex,
                          const std::string& file_name,
                          size_t const       max_msg)
    : mutex_(mutex)
    , file_name_(file_name)
    , template_(new char [file_name_.length() + TEMP_EXTENSION.length() + 1])
    , state_(wsrep::reporter::s_disconnected_disconnected)
    , progress_(indefinite)
    , initialized_(false)
    , err_msg_()
    , warn_msg_()
    , max_msg_(max_msg)
{
    template_[file_name_.length() + TEMP_EXTENSION.length()] = '\0';
    write_file(timestamp());
}

wsrep::reporter::~reporter()
{
    delete [] template_;
}

wsrep::reporter::substates
wsrep::reporter::substate_map(enum wsrep::server_state::state const state)
{
    switch (state)
    {
    case wsrep::server_state::s_disconnected:
        initialized_ = false;
        return s_disconnected_disconnected;
    case wsrep::server_state::s_initializing:
        if (s_disconnected_disconnected == state_)
            return s_disconnected_initializing;
        else if (s_joining_sst == state_)
            return s_joining_initializing;
        else if (s_joining_initializing == state_)
            return s_joining_initializing; // continuation
        else
        {
            assert(0);
            return state_;
        }
    case wsrep::server_state::s_initialized:
        initialized_ = true;
        if (s_disconnected_initializing >= state_)
            return s_disconnected_initialized;
        else if (s_joining_initializing == state_)
            return s_joining_ist;
        else if (s_joining_ist == state_)
            return s_joining_ist; // continuation
        else
        {
            assert(0);
            return state_;
        }
    case wsrep::server_state::s_connected:
        return s_connected_waiting;
    case wsrep::server_state::s_joiner:
        if (initialized_)
            return s_joining_initialized;
        else
            return s_joining_sst;
    case wsrep::server_state::s_joined:
        return s_joined_syncing;
    case wsrep::server_state::s_donor:
        return s_donor_sending;
    case wsrep::server_state::s_synced:
        return s_synced_running;
    case wsrep::server_state::s_disconnecting:
        return s_disconnecting_disconnecting;
    default:
        assert(0);
        return state_;
    }
}

static float const SST_SHARE  = 0.5f; // SST share of JOINING progress
static float const INIT_SHARE = 0.1f; // initialization share of JOINING progress
static float const IST_SHARE  = (1.0f - SST_SHARE - INIT_SHARE); // IST share

float
wsrep::reporter::progress_map(float const progress) const
{
    assert(progress >= 0.0f);
    assert(progress <= 1.0f);

    switch (state_)
    {
    case s_disconnected_disconnected:
        return indefinite;
    case s_disconnected_initializing:
        return indefinite;
    case s_disconnected_initialized:
        return indefinite;
    case s_connected_waiting:
        return indefinite;
    case s_joining_initialized:
        return progress;
    case s_joining_sst:
        return progress * SST_SHARE;
    case s_joining_initializing:
        return SST_SHARE + progress * INIT_SHARE;
    case s_joining_ist:
        return SST_SHARE + INIT_SHARE + progress * IST_SHARE;
    case s_joined_syncing:
        return progress;
    case s_synced_running:
        return 1.0;
    case s_donor_sending:
        return progress;
    case s_disconnecting_disconnecting:
        return indefinite;
    default:
        assert(0);
        return progress;
    }
}

void
wsrep::reporter::write_log_msg(std::ostream&  os,
                               const log_msg& msg)
{
    os << "\t\t{\n";
    os << "\t\t\t\"timestamp\": " << std::showpoint << std::setprecision(18)
       << msg.tstamp << ",\n";
    os << "\t\t\t\"msg\": \"" << msg.msg << "\"\n";
    os << "\t\t}";
}

void
wsrep::reporter::write_log_msgs(std::ostream&              os,
                                const std::string&         label,
                                const std::deque<log_msg>& msgs)
{
    os << "\t\"" << label << "\": [\n";
    for (size_t i(0); i < msgs.size(); ++i)
    {
        write_log_msg(os, msgs[i]);
        os << (i+1 < msgs.size() ? ",\n" : "\n");
    }
    os << "\t],\n";
}

// write data to temporary file and then rename it to target file for atomicity
void
wsrep::reporter::write_file(double const tstamp)
{
    enum progress_type {
        t_indefinite = -1, // indefinite wait
        t_progressive,     // measurable progress
        t_final            // final state
    };

    struct strings {
        const char*   state;
        const char*   comment;
        progress_type type;
    };

    static const struct strings strings[substates_max] =
        {
            { "DISCONNECTED",  "Disconnected",    t_indefinite  },
            { "DISCONNECTED",  "Initializing",    t_indefinite  },
            { "DISCONNECTED",  "Connecting",      t_indefinite  },
            { "CONNECTED",     "Waiting",         t_indefinite  },
            { "JOINING",       "Receiving state", t_progressive },
            { "JOINING",       "Receiving SST",   t_progressive },
            { "JOINING",       "Initializing",    t_progressive },
            { "JOINING",       "Receiving IST",   t_progressive },
            { "JOINED",        "Syncing",         t_progressive },
            { "SYNCED",        "Operational",     t_final       },
            { "DONOR",         "Donating SST",    t_progressive },
            { "DISCONNECTING", "Disconnecting",   t_indefinite  }
        };

    // prepare template for mkstemp()
    file_name_.copy(template_, file_name_.length());
    TEMP_EXTENSION.copy(template_ +file_name_.length(),TEMP_EXTENSION.length());

    int const fd(mkstemp(template_));
    if (fd < 0)
    {
        std::cerr << "Reporter could not open temporary file `" << template_
                  << "': " << strerror(errno) << " (" << errno << ")\n";
        return;
    }

    double const seconds(floor(tstamp));
    time_t const tt = time_t(seconds);
    struct tm    date;
    localtime_r(&tt, &date);

    char date_str[85] = { '\0', };
    snprintf(date_str, sizeof(date_str) - 1,
             "%04d-%02d-%02d %02d:%02d:%02d.%03d",
             date.tm_year + 1900, date.tm_mon + 1, date.tm_mday,
             date.tm_hour, date.tm_min, date.tm_sec,
             (int)((tstamp-seconds)*1000));

    std::ostringstream os;
    os << "{\n";
    os << "\t\"date\": \"" << date_str << "\",\n";
    os << "\t\"timestamp\": " << std::showpoint << std::setprecision(18)
       << tstamp << ",\n";
    write_log_msgs(os, "errors",   err_msg_);
    write_log_msgs(os, "warnings", warn_msg_);
    os << "\t\"status\": {\n";
    os << "\t\t\"state\": \"" << strings[state_].state << "\",\n";
    os << "\t\t\"comment\": \"" << strings[state_].comment << "\",\n";
    os << "\t\t\"progress\": " << std::showpoint << std::setprecision(6)
       << progress_ << "\n";
    os << "\t}\n";
    os << "}\n";

    std::string str(os.str());
    ssize_t err(write(fd, str.c_str(), str.length()));
    if (err < 0)
    {
        std::cerr << "Could not write " << str.length()
                  << " bytes to temporary file '"
                  << template_ << "': " << strerror(errno)
                  << " (" << errno << ")\n";
        return;
    }

    rename(template_, file_name_.c_str());
}

void
wsrep::reporter::report_state(enum server_state::state const s, float const p)
{
    assert(p >= -1);
    assert(p <= 1);

    bool flush(false);

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);

    substates const state(substate_map(s));

    if (state != state_)
    {
        state_ = state;
        flush = true;
    }

    float const progress(progress_map(p));
    assert(progress >= -1);
    assert(progress <= 1);

    if (progress != progress_)
    {
        progress_ = progress;
        flush = true;
    }

    if (flush)
    {
        write_file(timestamp());
    }
}

void
wsrep::reporter::report_log_msg(log_level const    lvl,
                                const std::string& msg,
                                double             tstamp)
{
    std::deque<log_msg>& deque(lvl == error ? err_msg_ : warn_msg_);

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);

    if (deque.empty() || msg != deque.back().msg)
    {
        if (deque.size() == max_msg_) deque.pop_front();

        if (tstamp <= undefined) tstamp = timestamp();

        log_msg entry({tstamp, msg});
        deque.push_back(entry);
        write_file(tstamp);
    }
}
