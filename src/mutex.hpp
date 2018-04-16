//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MUTEX_HPP
#define TRREP_MUTEX_HPP

#include "exception.hpp"

#include <pthread.h>

namespace trrep
{
    // Default pthread implementation

    class mutex
    {
    public:
        mutex()
            : mutex_()
        {
            if (pthread_mutex_init(&mutex_, 0))
            {
                throw trrep::runtime_error("mutex init failed");
            }
        }
        ~mutex()
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
        mutex(const mutex& other);
        mutex& operator=(const mutex& other);
        pthread_mutex_t mutex_;
    };
}

#endif // TRREP_MUTEX_HPP
