/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WSREP_LOCK_HPP
#define WSREP_LOCK_HPP

#include "assert.hpp"
#include "mutex.hpp"

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
            WSREP_ASSERT(locked_ == false);
            locked_ = true;
        }

        void unlock()
        {
            WSREP_ASSERT(locked_);
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
