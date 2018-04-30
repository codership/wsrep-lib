//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_LOGGER_HPP
#define TRREP_LOGGER_HPP

#include "mutex.hpp"
#include "lock.hpp"

#include <iosfwd>
#include <sstream>

namespace trrep
{
    class log
    {
    public:
        log()
            : oss_()
        { }
        ~log()
        {
            trrep::unique_lock<trrep::mutex> lock(mutex_);
            os_ << oss_.str() << "\n";
        }
        template <typename T>
        std::ostream& operator<<(const T& val)
        {
            return (oss_ << val);
        }
    private:
        std::ostringstream oss_;
        static trrep::mutex& mutex_;
        static std::ostream& os_;
    };
}

#endif // TRREP_LOGGER_HPP
