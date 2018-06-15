//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_params.hpp"

#include <boost/program_options.hpp>
#include <iostream>

db::params db::parse_args(int argc, char** argv)
{
    namespace po = boost::program_options;
    db::params params;
    po::options_description desc("Allowed options");
    desc.add_options()
        ("help", "produce help message")
        ("wsrep-provider",
         po::value<std::string>(&params.wsrep_provider)->required(),
         "wsrep provider to load")
        ("wsrep-provider-options",
         po::value<std::string>(&params.wsrep_provider_options),
         "wsrep provider options")
        ("servers", po::value<size_t>(&params.n_servers)->required(),
         "number of servers to start")
        ("clients", po::value<size_t>(&params.n_clients)->required(),
         "number of clients to start per server")
        ("transactions", po::value<size_t>(&params.n_transactions),
         "number of transactions run by a client")
        ("rows", po::value<size_t>(&params.n_rows),
         "number of rows per table")
        ("alg-freq", po::value<size_t>(&params.alg_freq),
         "ALG frequency")
        ("debug-log-level", po::value<int>(&params.debug_log_level),
         "debug logging level: 0 - none, 1 - verbose")
        ("fast-exit", po::value<int>(&params.fast_exit),
         "exit from simulation without graceful shutdown");
    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);
    if (vm.count("help"))
    {
        std::cerr << desc << "\n";
        exit(0);
    }
    return params;
}
