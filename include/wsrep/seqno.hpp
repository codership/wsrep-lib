//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_SEQNO_HPP
#define WSREP_SEQNO_HPP

#include "exception.hpp"

#include <ostream>

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
            : seqno_()
        { }

        explicit seqno(long long seqno)
            : seqno_(seqno)
        {
            if (seqno_ < 0)
            {
                throw wsrep::runtime_error("Negative seqno given");
            }
        }

        long long get() const
        {
            return seqno_;
        }

        bool nil() const
        {
            return (seqno_ == 0);
        }

        bool operator<(seqno other) const
        {
            return (seqno_ < other.seqno_);
        }

        bool operator>(seqno other) const
        {
            return (seqno_ > other.seqno_);
        }

        seqno operator+(seqno other) const
        {
            return (seqno(seqno_ + other.seqno_));
        }

        static seqno undefined() { return seqno(0); }
    private:
        long long seqno_;
    };

    static inline std::ostream& operator<<(std::ostream& os, wsrep::seqno seqno)
    {
        return (os << seqno.get());
    }
}

#endif // WSREP_SEQNO_HPP
