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

/** @file info.hpp
 *
 * Interface to report application status to external programs
 * via JSON file.
 */

#ifndef WSREP_REPORTER_HPP
#define WSREP_REPORTER_HPP

#include "mutex.hpp"
#include "server_state.hpp"

#include <string>
#include <deque>

namespace wsrep
{
    class reporter
    {
    public:
        reporter(mutex&             mutex,
                 const std::string& file_name,
                 size_t             max_msg);

        virtual ~reporter();

        // indefinite progress value
        static float constexpr indefinite = -1.0f;

        void report_state(enum server_state::state state,
                          float progress = indefinite);

        enum log_level
        {
            error,
            warning
        };

        // undefined timestamp value
        static double constexpr undefined = 0.0;

        void report_log_msg(log_level, const std::string& msg,
                            double timestamp = undefined);

    private:
        enum substates {
            s_disconnected_disconnected,
            s_disconnected_initializing,
            s_disconnected_initialized,
            s_connected_waiting, // to become joiner
            s_joining_initialized,
            s_joining_sst,
            s_joining_initializing,
            s_joining_ist,
            s_joined_syncing,
            s_synced_running,
            s_donor_sending,
            s_disconnecting_disconnecting,
            substates_max
        };

        wsrep::mutex&       mutex_;
        std::string const   file_name_;
        char*               template_;
        substates           state_;
        float               progress_;
        bool                initialized_;

        typedef struct {
            double tstamp;
            std::string msg;
        } log_msg;

        std::deque<log_msg> err_msg_;
        std::deque<log_msg> warn_msg_;
        size_t const        max_msg_;

        static void write_log_msgs(std::ostream& os,
                                   const std::string& label,
                                   const std::deque<log_msg>& msgs);
        static void write_log_msg(std::ostream& os,
                                   const log_msg& msg);

        substates substate_map(enum server_state::state state);
        float     progress_map(float progress) const;
        void      write_file(double timestamp);

        // make uncopyable
        reporter(const wsrep::reporter&);
        void operator=(const wsrep::reporter&);
    }; /* reporter */

} /* wsrep */

#endif /* WSREP_REPORTER_HPP */
