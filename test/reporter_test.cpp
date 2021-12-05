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

#include <boost/test/unit_test.hpp>

#include <boost/json/src.hpp>

#include <fstream>
#include <deque>
#include <vector>
#include <unistd.h> // unlink() for cleanup

namespace json = boost::json;

//////// HELPERS ///////

static inline double
timestamp()
{
    struct timespec time;
    clock_gettime(CLOCK_REALTIME, &time);
    return (double(time.tv_sec) + double(time.tv_nsec)*1.0e-9);
}

static std::string make_progress_string(int const from, int const to,
                                        int const total,int const done,
                                        int const indefinite)
{
    std::ostringstream os;

    os << "{ \"from\": "     << from       << ", "
       << "\"to\": "         << to         << ", "
       << "\"total\": "      << total      << ", "
       << "\"done\": "       << done       << ", "
       << "\"indefinite\": " << indefinite << " }";

    return os.str();
}


static json::value
read_file(const char* const filename)
{
    std::ifstream       input(filename, std::ios::binary);
    std::vector<char>   buffer(std::istreambuf_iterator<char>(input), {});
    json::stream_parser parser;
    json::error_code    err;

    parser.write(buffer.data(), buffer.size(), err);
    if (err)
    {
        assert(0);
        return nullptr;
    }

    parser.finish(err);
    if (err)
    {
        assert(0);
        return nullptr;
    }

    return parser.release();
}

struct logs
{
    std::deque<double>      tstamp_;
    std::deque<std::string> msg_;
};

struct progress
{
    int from_;
    int to_;
    int total_;
    int done_;
    int indefinite_;

    bool indefinite() const { return total_ == indefinite_; }
    bool steady()     const { return total_ == 0; }
    progress(int const from, int const to, int const total, int const done,
        int const indefinite)
        : from_(from)
        , to_(to)
        , total_(total)
        , done_(done)
        , indefinite_(indefinite)
    {}
};

static bool
operator==(const progress& left, const progress& right)
{
    if (left.indefinite() && right.indefinite()) return true;
    if (left.steady() && right.steady()) return true;

    return (left.from_       == right.from_ &&
            left.to_         == right.to_ &&
            left.total_      == right.total_ &&
            left.done_       == right.done_ &&
            left.indefinite_ == right.indefinite_);
}

static bool
operator!=(const progress& left, const progress& right)
{
    return !(left == right);
}

static std::ostream&
operator<<(std::ostream& os, const progress& p)
{
    os << make_progress_string(p.from_, p.to_, p.total_, p.done_,p.indefinite_);
    return os;
}

struct result
{
    logs errors_;
    logs warnings_;
    struct
    {
        std::string state_;
        std::string comment_;
        progress    progress_;
    } status_;
};

static void
parse_result(const json::value& value, struct result& res,
             const std::string& path = "")
{
    //std::cout << "Parsing " << path << ": " << value << ": " << value.kind() << std::endl;
    switch (value.kind())
    {
    case json::kind::object:
    {
        auto const obj(value.get_object());
        if (!obj.empty())
        {
            for (auto it = obj.begin(); it != obj.end(); ++it)
            {
                std::string const key(it->key().data(), it->key().length());
                parse_result(it->value(), res, path + "." + key);
            }
        }
        return;
    }
    case json::kind::array:
    {
        auto const arr(value.get_array());
        if (!arr.empty())
        {
            for (auto it = arr.begin(); it != arr.end(); ++it)
            {
                parse_result(*it, res, path + ".[]");
            }
        }
        return;
    }
    case json::kind::string:
    {
        auto const val(value.get_string().c_str());
        if (path == ".errors.[].msg")
        {
            res.errors_.msg_.push_back(val);
        }
        else if (path == ".warnings.[].msg")
        {
            res.warnings_.msg_.push_back(val);
        }
        else if (path == ".status.state")
        {
            res.status_.state_ = val;
        }
        else if (path == ".status.comment")
        {
            res.status_.comment_ = val;
        }
        return;
    }
    case json::kind::uint64:
        WSREP_FALLTHROUGH;
    case json::kind::int64:
        if (path == ".status.progress.from")
        {
            res.status_.progress_.from_ = int(value.get_int64());
        }
        else if (path == ".status.progress.to")
        {
            res.status_.progress_.to_ = int(value.get_int64());
        }
        else if (path == ".status.progress.total")
        {
            res.status_.progress_.total_ = int(value.get_int64());
        }
        else if (path == ".status.progress.done")
        {
            res.status_.progress_.done_ = int(value.get_int64());
        }
        else if (path == ".status.progress.indefinite")
        {
            res.status_.progress_.indefinite_ = int(value.get_int64());
        }
        return;
    case json::kind::double_:
        if (path == ".errors.[].timestamp")
        {
            res.errors_.tstamp_.push_back(value.get_double());
        }
        else if (path == ".warnings.[].timestamp")
        {
            res.warnings_.tstamp_.push_back(value.get_double());
        }

        return;
    case json::kind::bool_:
        return;
    case json::kind::null:
        return;
    }

    assert(0);
}

static bool
equal(const std::string& left, const std::string& right)
{
    return left == right;
}

static bool
equal(double const left, double const right)
{
    return ::fabs(left - right) < 0.0001; // we are looking for ms precision
}

template <typename T>
static bool
operator!=(const std::deque<T>& left, const std::deque<T>& right)
{
    if (left.size() != right.size()) return true;

    for (size_t i(0); i < left.size(); ++i)
        if (!equal(left[i], right[i])) return true;

    return false;
}

static bool
operator!=(const logs& left, const logs& right)
{
    if (left.tstamp_ != right.tstamp_) return true;
    return (left.msg_ != right.msg_);
}

static bool
operator==(const result& left, const result& right)
{
    if (left.errors_   != right.errors_)   return false;
    if (left.warnings_ != right.warnings_) return false;

    if (left.status_.state_    != right.status_.state_)    return false;
    if (left.status_.comment_  != right.status_.comment_)  return false;
    if (left.status_.progress_ != right.status_.progress_) return false;

    return true;
}

template <typename T>
static void
print_deque(std::ostream& os,const std::deque<T> left,const std::deque<T> right)
{
    auto const max(std::max(left.size(), right.size()));
    for (size_t i(0); i < max; ++i)
    {
        os << "|\t'";

        if (i < left.size())
            os << left[i] << "'";
        else
            os << "'\t";

        if (i < right.size())
            os << "\t'" << right[i] << "'";
        else
            os << "\t''";

        if (!equal(left[i], right[i])) os << "\t!!!";

        os << "\n";
    }
}

static void
print_logs(std::ostream& os, const logs& left, const logs& right)
{
    os << "|\t" << left.msg_.size() << "\t" << right.msg_.size() << "\n";
    print_deque(os, left.tstamp_, right.tstamp_);
    print_deque(os, left.msg_, right.msg_);
}

// print two results against each other
static std::string
print(const result& left, const result& right, size_t it)
{
    std::ostringstream os;

    os << std::showpoint << std::setprecision(18);

    os << "Iteration " << it << "\nerrors:\n";
    print_logs(os, left.errors_, right.errors_);
    os << "warnings:\n";
    print_logs(os, left.warnings_, right.warnings_);
    os << "state:\n";
    os << "\t" << left.status_.state_   << "\t" << right.status_.state_
       << "\n";
    os << "\t" << left.status_.comment_ << "\t" << right.status_.comment_
       << "\n";
    os << "\t" << left.status_.progress_ << "\t" << right.status_.progress_
       << "\n";

    return os.str();
}

#define VERIFY_RESULT(left, right, it)                          \
    BOOST_CHECK_MESSAGE(left == right, print(left, right, it));

static const char*
const REPORT = "report.json";

static progress
const indefinite(-1, -1, -1, -1, -1);

static progress
const steady(-1, -1, 0, 0, -1);

static struct logs
const LOGS_INIT = { std::deque<double>(), std::deque<std::string>() };
static struct result
const RES_INIT = { LOGS_INIT, LOGS_INIT,
                  { "DISCONNECTED", "Disconnected", indefinite } };

static void
test_log(const char* const  fname,
         wsrep::reporter&   rep,
         result&            check,
         wsrep::reporter::log_level const lvl,
         double const       tstamp,
         const std::string& msg,
         size_t const       iteration)
{
    // this is implementaiton detail, if it changes in the code, it needs
    // to be changed here
    size_t const MAX_ERROR(4);

    logs& log(lvl == wsrep::reporter::error ? check.errors_ : check.warnings_);
    log.tstamp_.push_back(tstamp);
    if (log.tstamp_.size() > MAX_ERROR) log.tstamp_.pop_front();
    log.msg_.push_back(msg);
    if (log.msg_.size() > MAX_ERROR) log.msg_.pop_front();

    rep.report_log_msg(lvl, msg, tstamp);

    auto value = read_file(fname);
    auto res = RES_INIT;
    parse_result(value, res);
    VERIFY_RESULT(res, check, iteration);
}

static size_t const MAX_MSG = 4;

BOOST_AUTO_TEST_CASE(log_msg_test)
{
    wsrep::default_mutex m;
    wsrep::reporter rep(m, REPORT, MAX_MSG);

    auto value = read_file(REPORT);
    BOOST_REQUIRE(value != nullptr);

    struct result res(RES_INIT), check(RES_INIT);
    parse_result(value, res);
    VERIFY_RESULT(res, check, -1);

    struct entry
    {
        double tstamp_;
        std::string msg_;
    };
    std::vector<entry> msgs =
        {
            { 0.1, "a"      },
            { 0.2, "bb"     },
            { 0.3, "ccc"    },
            { 0.4, "dddd"   },
            { 0.5, "eeeee"  },
            { 0.6, "ffffff" }
        };
    for (size_t i(0); i < msgs.size(); ++i)
    {
        test_log(REPORT, rep, check,
                 wsrep::reporter::error, msgs[i].tstamp_, msgs[i].msg_, i);
        test_log(REPORT, rep, check,
                 wsrep::reporter::warning, msgs[i].tstamp_, msgs[i].msg_, i);
    }

    // test indefinite timestmap
    std::string const msg("err");
    rep.report_log_msg(wsrep::reporter::error, msg);
    value = read_file(REPORT);
    res = RES_INIT;
    parse_result(value, res);
    BOOST_REQUIRE(res.errors_.tstamp_.back() > 0);
    BOOST_REQUIRE(res.errors_.msg_.back() == msg);

    ::unlink(REPORT);
}

BOOST_AUTO_TEST_CASE(state_test)
{
    using wsrep::server_state;

    wsrep::default_mutex m;
    wsrep::reporter   rep(m, REPORT, MAX_MSG);
    double const      err_tstamp(timestamp());
    std::string const err_msg("Error!");

    struct test
    {
        struct
        {
            enum wsrep::server_state::state state;
            float progress;
        } input;
        struct result output;
    };

    logs const ERRS_INIT = { {err_tstamp}, {err_msg} };

    std::vector<test> tests =
    {
        {{ server_state::s_disconnected,  0 },
         { ERRS_INIT, LOGS_INIT,
          { "DISCONNECTED",  "Disconnected",    indefinite }}},
        {{ server_state::s_initializing,  0 },
         { ERRS_INIT, LOGS_INIT,
          { "DISCONNECTED",  "Initializing",    indefinite }}},
        {{ server_state::s_initialized,   0 },
         { ERRS_INIT, LOGS_INIT,
          { "DISCONNECTED",  "Connecting",      indefinite }}},
        {{ server_state::s_connected,     0 },
         { ERRS_INIT, LOGS_INIT,
          { "CONNECTED",     "Waiting",         indefinite }}},
        {{ server_state::s_joiner,        0 },
         { ERRS_INIT, LOGS_INIT,
          { "JOINING",       "Receiving state", indefinite }}},
        {{ server_state::s_disconnecting, 0 },
         { ERRS_INIT, LOGS_INIT,
          { "DISCONNECTING", "Disconnecting",   indefinite }}},
        {{ server_state::s_disconnected,  0 },
         { ERRS_INIT, LOGS_INIT,
          { "DISCONNECTED",  "Disconnected",    indefinite }}},
        {{ server_state::s_connected,     0 },
         { ERRS_INIT, LOGS_INIT,
          { "CONNECTED",     "Waiting",         indefinite }}},
        {{ server_state::s_joiner,        0 },
         { ERRS_INIT, LOGS_INIT,
          { "JOINING",       "Receiving SST",   indefinite }}},
        {{ server_state::s_initializing,  0 },
         { ERRS_INIT, LOGS_INIT,
          { "JOINING",       "Initializing",    indefinite }}},
        {{ server_state::s_initialized,   0 },
         { ERRS_INIT, LOGS_INIT,
          { "JOINING",       "Receiving IST",   indefinite }}},
        {{ server_state::s_joined,        0 },
         { ERRS_INIT, LOGS_INIT,
          { "JOINED",        "Syncing",         indefinite }}},
        {{ server_state::s_synced,        0 },
         { ERRS_INIT, LOGS_INIT,
          { "SYNCED",        "Operational",     steady     }}},
        {{ server_state::s_donor,         0 },
         { ERRS_INIT, LOGS_INIT,
          { "DONOR",         "Donating SST",    indefinite }}},
        {{ server_state::s_joined,        0 },
         { ERRS_INIT, LOGS_INIT,
          { "JOINED",        "Syncing",         indefinite }}},
        {{ server_state::s_synced,        0 },
         { ERRS_INIT, LOGS_INIT,
          { "SYNCED",        "Operational",     steady     }}},
        {{ server_state::s_disconnecting, 0 },
         { ERRS_INIT, LOGS_INIT,
          { "DISCONNECTING", "Disconnecting",   indefinite }}},
    };

    rep.report_log_msg(wsrep::reporter::error, err_msg, err_tstamp);

    for (auto i(tests.begin()); i != tests.end(); ++i)
    {
        rep.report_state(i->input.state);
        auto value = read_file(REPORT);
        result res(RES_INIT);
        parse_result(value, res);
        auto check(i->output);
        VERIFY_RESULT(res, check, i - tests.begin());
    }

    ::unlink(REPORT);
}

BOOST_AUTO_TEST_CASE(progress_test)
{
    using wsrep::server_state;

    wsrep::default_mutex m;
    wsrep::reporter   rep(m, REPORT, MAX_MSG);
    double const      warn_tstamp(timestamp());
    std::string const warn_msg("Warn!");

    static progress const progress5_0(-1, -1, 5, 0, -1);
    static progress const progress5_2(-1, -1, 5, 2, -1);
    static progress const progress5_5(-1, -1, 5, 5, -1);

    struct test
    {
        struct
        {
            enum wsrep::server_state::state state;
            int total;
            int done;
        } input;
        struct result output;
    };

    logs const WARN_INIT = { {warn_tstamp}, {warn_msg} };

    std::vector<test> tests =
    {
        {{ server_state::s_initialized,  -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "DISCONNECTED", "Connecting",      indefinite  }}},
        {{ server_state::s_connected,    -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "CONNECTED",    "Waiting",         indefinite  }}},
        {{ server_state::s_joiner,        5,  0 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving state", progress5_0 }}},
        {{ server_state::s_joiner,        5,  2 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving state", progress5_2 }}},
        {{ server_state::s_disconnected, -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "DISCONNECTED", "Disconnected",    indefinite  }}},
        {{ server_state::s_connected,    -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "CONNECTED",    "Waiting",         indefinite  }}},
        {{ server_state::s_joiner,        5,  0 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving SST",   progress5_0 }}},
        {{ server_state::s_joiner,        5,  2 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving SST",   progress5_2 }}},
        {{ server_state::s_joiner,        5,  5 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving SST",   progress5_5 }}},
        {{ server_state::s_initializing, -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Initializing",    indefinite  }}},
        {{ server_state::s_initializing,  5,  2 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Initializing",    progress5_2 }}},
        {{ server_state::s_initialized,   5,  0 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving IST",   progress5_0 }}},
        {{ server_state::s_initialized,   5, 5 },
         { LOGS_INIT, WARN_INIT,
          { "JOINING",      "Receiving IST",   progress5_5 }}},
        {{ server_state::s_joined,       -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "JOINED",       "Syncing",         indefinite  }}},
        {{ server_state::s_joined,        5, 2 },
         { LOGS_INIT, WARN_INIT,
          { "JOINED",       "Syncing",         progress5_2 }}},
        {{ server_state::s_synced,       -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "SYNCED",       "Operational",     steady      }}},
        {{ server_state::s_donor,        -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "DONOR",        "Donating SST",    indefinite  }}},
        {{ server_state::s_donor,         5,  0 },
         { LOGS_INIT, WARN_INIT,
          { "DONOR",        "Donating SST",    progress5_0 }}},
        {{ server_state::s_joined,       -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "JOINED",       "Syncing",         indefinite }}},
        {{ server_state::s_synced,       -1, -1 },
         { LOGS_INIT, WARN_INIT,
          { "SYNCED",       "Operational",     steady     }}},
    };

    rep.report_log_msg(wsrep::reporter::warning, warn_msg, warn_tstamp);

    for (auto i(tests.begin()); i != tests.end(); ++i)
    {
        rep.report_state(i->input.state);
        if (i->input.total >= 0)
            rep.report_progress(make_progress_string(-1, -1,
                                                 i->input.total, i->input.done,
                                                 -1));
        auto value = read_file(REPORT);
        result res(RES_INIT);
        parse_result(value, res);
        auto check(i->output);
        VERIFY_RESULT(res, check, i - tests.begin());
    }

    ::unlink(REPORT);
}
