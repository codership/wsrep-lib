//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_LOCK_HPP
#define TRREP_LOCK_HPP

#include "mutex.hpp"

#include <cassert>

namespace trrep
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
            assert(locked_);
            mutex_.unlock();
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

#endif // TRREP_LOCK_HPP
