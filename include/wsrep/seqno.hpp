//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_SEQNO_HPP
#define WSREP_SEQNO_HPP

#include "exception.hpp"

#include <iosfwd>

namespace wsrep
{
    /** @class seqno
     *
     * Sequence number type.
     *
     * By convention, nil value is zero, negative values are not allowed.
     * Relation operators are restricted to < and > on purpose to
     * enforce correct use.
     */
    class seqno
    {
    public:
        seqno()
            : seqno_(-1)
        { }

        explicit seqno(long long seqno)
            : seqno_(seqno)
        {
            if (seqno_ < -1)
            {
                throw wsrep::runtime_error("Too negative seqno given");
            }
        }

        long long get() const
        {
            return seqno_;
        }

        bool is_undefined() const
        {
            return (seqno_ == -1);
        }

        bool operator<(seqno other) const
        {
            return (seqno_ < other.seqno_);
        }

        bool operator>(seqno other) const
        {
            return (seqno_ > other.seqno_);
        }
        bool operator==(seqno other) const
        {
            return (seqno_ == other.seqno_);
        }
        seqno operator+(seqno other) const
        {
            return (seqno(seqno_ + other.seqno_));
        }
        seqno operator+(long long other) const
        {
            return (*this + seqno(other));
        }
        static seqno undefined() { return seqno(-1); }
    private:
        long long seqno_;
    };

    std::ostream& operator<<(std::ostream& os, wsrep::seqno seqno);
}

#endif // WSREP_SEQNO_HPP
