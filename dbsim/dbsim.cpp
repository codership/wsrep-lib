//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_params.hpp"
#include "db_simulator.hpp"

int main(int argc, char** argv)
{
    db::simulator(db::parse_args(argc, argv)).run();
    return 0;
}
