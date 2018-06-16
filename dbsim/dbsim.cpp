//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_params.hpp"
#include "db_simulator.hpp"

int main(int argc, char** argv)
{
    try
    {
        db::simulator(db::parse_args(argc, argv)).run();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << std::endl;
        return 1;
    }
    return 0;
}
