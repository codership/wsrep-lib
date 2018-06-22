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
        enum level
        {
            debug,
            info,
            warning,
            error
        };
        static const char* to_c_string(enum level level)
        {
            switch (level)
            {
            case debug:   return "debug";
            case info:    return "info";
            case warning: return "warning";
            case error:   return "error";
            };
            return "unknown";
        }
        log(enum wsrep::log::level level, const char* prefix = "")
            : level_(level)
            , prefix_(prefix)
            , oss_()
        { }
        ~log()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            os_ << prefix_ << ": " << oss_.str() << std::endl;
        }
        template <typename T>
        std::ostream& operator<<(const T& val)
        {
            return (oss_ << val);
        }
    private:
        log(const log&);
        log& operator=(const log&);
        enum level level_;
        const char* prefix_;
        std::ostringstream oss_;
        static wsrep::mutex& mutex_;
        static std::ostream& os_;
    };

    class log_error : public log
    {
    public:
        log_error()
            : log(error) { }
    };

    class log_warning : public log
    {
    public:
        log_warning()
            : log(warning) { }
    };

    class log_info : public log
    {
    public:
        log_info()
            : log(info) { }
    };

    class log_debug : public log
    {
    public:
        log_debug()
            : log(debug) { }
    };
}

#endif // WSREP_LOGGER_HPP
