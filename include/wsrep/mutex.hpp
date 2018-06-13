//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MUTEX_HPP
#define WSREP_MUTEX_HPP

#include "exception.hpp"

#include <pthread.h>

namespace wsrep
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
        /* Return native handle */
        virtual void* native() = 0;
    private:
        mutex(const mutex& other);
        mutex& operator=(const mutex& other);
    };

    // Default pthread implementation
    class default_mutex : public wsrep::mutex
    {
    public:
        default_mutex()
            : wsrep::mutex(),
              mutex_()
        {
            if (pthread_mutex_init(&mutex_, 0))
            {
                throw wsrep::runtime_error("mutex init failed");
            }
        }
        ~default_mutex()
        {
            if (pthread_mutex_destroy(&mutex_)) ::abort();
        }

        void lock()
        {
            if (pthread_mutex_lock(&mutex_))
            {
                throw wsrep::runtime_error("mutex lock failed");
            }
        }

        void unlock()
        {
            if (pthread_mutex_unlock(&mutex_))
            {
                throw wsrep::runtime_error("mutex unlock failed");
            }
        }

        void* native()
        {
            return &mutex_;
        }

    private:
        pthread_mutex_t mutex_;
    };
}

#endif // WSREP_MUTEX_HPP
