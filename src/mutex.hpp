//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MUTEX_HPP
#define TRREP_MUTEX_HPP

#include "exception.hpp"

#include <pthread.h>

namespace trrep
{
    //!
    //! 
    //!
    class mutex
    {
    public:
        mutex() { }
        virtual ~mutex() { }
        virtual void lock() = 0;
        virtual void unlock() = 0;
    private:
        mutex(const mutex& other);
        mutex& operator=(const mutex& other);
    };

    // Default pthread implementation
    class default_mutex : public trrep::mutex
    {
    public:
        default_mutex()
            : trrep::mutex(),
              mutex_()
        {
            if (pthread_mutex_init(&mutex_, 0))
            {
                throw trrep::runtime_error("mutex init failed");
            }
        }
        ~default_mutex()
        {
            if (pthread_mutex_destroy(&mutex_))
            {
                throw trrep::runtime_error("mutex destroy failed");
            }
        }

        void lock()
        {
            if (pthread_mutex_lock(&mutex_))
            {
                throw trrep::runtime_error("mutex lock failed");
            }
        }

        void unlock()
        {
            if (pthread_mutex_unlock(&mutex_))
            {
                throw trrep::runtime_error("mutex unlock failed");
            }
        }
    private:
        pthread_mutex_t mutex_;
    };
}

#endif // TRREP_MUTEX_HPP
