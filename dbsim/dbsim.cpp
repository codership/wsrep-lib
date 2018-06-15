//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_params.hpp"
#include "db_simulator.hpp"

int main(int argc, char** argv)
{
    db::params params(db::parse_args(argc, argv));
    db::simulator simulator(params);
    simulator.start();
    simulator.stop();
    std::cout << simulator.stats() << std::endl;
    return 0;
}
