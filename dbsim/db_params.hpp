/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
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

#ifndef WSREP_DB_PARAMS_HPP
#define WSREP_DB_PARAMS_HPP

#include <cstddef>
#include <string>

namespace db
{
    struct params
    {
        size_t n_servers{0};
        size_t n_clients{0};
        size_t n_transactions{0};
        size_t n_rows{1000};
        size_t max_data_size{8}; // Maximum size of write set data payload.
        bool random_data_size{false}; // If true, randomize data payload size.
        /* Asymmetric lock granularity frequency. */
        size_t alg_freq{0};
        /* Whether to sync wait before start of transaction. */
        bool sync_wait{false};
        std::string topology{};
        std::string wsrep_provider{};
        std::string wsrep_provider_options{};
        std::string status_file{"status.json"};
        int debug_log_level{0};
        int fast_exit{0};
        int thread_instrumentation{0};
        bool cond_checks{false};
        int tls_service{0};
        bool check_sequential_consistency{false};
        bool do_2pc{false};
    };

    params parse_args(int argc, char** argv);
}

#endif // WSREP_DB_PARAMS_HPP
