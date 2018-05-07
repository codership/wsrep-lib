//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_EXCEPTION_HPP
#define TRREP_EXCEPTION_HPP

#include <stdexcept>
#include <cstdlib>

namespace trrep
{
    class runtime_error : public std::runtime_error
    {
    public:
        runtime_error(const std::string& msg)
            : std::runtime_error(msg)
        {
            ::abort();
        }
    };

    class not_implemented_error : public std::exception
    {
    public:
        not_implemented_error()
            : std::exception()
        {
            ::abort();
        }
    };

}


#endif // TRREP_EXCEPTION_HPP
