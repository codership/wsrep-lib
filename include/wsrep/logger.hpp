//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_LOGGER_HPP
#define WSREP_LOGGER_HPP

#include "mutex.hpp"
#include "lock.hpp"

#include <iosfwd>
#include <sstream>

namespace wsrep
{
    class log
    {
    public:
        log(const std::string& prefix = "INFO")
            : prefix_(prefix)
            , oss_()
        { }
        ~log()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            os_ << prefix_ << ": " << oss_.str() << "\n";
        }
        template <typename T>
        std::ostream& operator<<(const T& val)
        {
            return (oss_ << val);
        }
    private:
        const std::string prefix_;
        std::ostringstream oss_;
        static wsrep::mutex& mutex_;
        static std::ostream& os_;
    };


    class log_debug : public log
    {
    public:
        log_debug()
            : log("DEBUG") { }
    };
}

#endif // WSREP_LOGGER_HPP
