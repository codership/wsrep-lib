//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_PARAMS_HPP
#define WSREP_DB_PARAMS_HPP

#include <cstddef>
#include <string>

namespace db
{
    struct params
    {
        size_t n_servers;
        size_t n_clients;
        size_t n_transactions;
        size_t n_rows;
        size_t alg_freq;
        std::string wsrep_provider;
        std::string wsrep_provider_options;
        int debug_log_level;
        int fast_exit;
        params()
            : n_servers(0)
            , n_clients(0)
            , n_transactions(0)
            , n_rows(1000)
            , alg_freq(0)
            , wsrep_provider()
            , wsrep_provider_options()
            , debug_log_level(0)
            , fast_exit(0)
        { }
    };

    params parse_args(int argc, char** argv);
}

#endif // WSREP_DB_PARAMS_HPP
