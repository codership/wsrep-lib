//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_LOCK_HPP
#define WSREP_LOCK_HPP

#include "mutex.hpp"

#include <cassert>

namespace wsrep
{
    template <class M>
    class unique_lock
    {
    public:
        unique_lock(M& mutex)
            : mutex_(mutex)
            , locked_(false)
        {
            mutex_.lock();
            locked_ = true;
        }
        ~unique_lock()
        {
            if (locked_)
            {
                unlock();
            }
        }

        void lock()
        {
            mutex_.lock();
            assert(locked_ == false);
            locked_ = true;
        }

        void unlock()
        {
            assert(locked_);
            locked_ = false;
            mutex_.unlock();
        }

        bool owns_lock() const
        {
            return locked_;
        }

        M& mutex() { return mutex_; }
    private:
        unique_lock(const unique_lock&);
        unique_lock& operator=(const unique_lock&);
        M& mutex_;
        bool locked_;
    };
}

#endif // WSREP_LOCK_HPP
