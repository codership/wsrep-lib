//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_EXCEPTION_HPP
#define WSREP_EXCEPTION_HPP

#include <stdexcept>
#include <cstdlib>

namespace wsrep
{
    extern bool abort_on_exception;

    class runtime_error : public std::runtime_error
    {
    public:
        runtime_error(const std::string& msg)
            : std::runtime_error(msg)
        {
            if (abort_on_exception)
            {
                ::abort();
            }
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

    class fatal_error : public std::exception
    {
    };
}


#endif // WSREP_EXCEPTION_HPP
