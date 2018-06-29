//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_GTID_HPP
#define WSREP_GTID_HPP

#include "id.hpp"
#include "seqno.hpp"

#include <iosfwd>

namespace wsrep
{
    class gtid
    {
    public:
        gtid()
            : id_(wsrep::id::undefined())
            , seqno_(wsrep::seqno::undefined())
        { }
        gtid(const wsrep::id& id, wsrep::seqno seqno)
            : id_(id)
            , seqno_(seqno)
        { }
        const wsrep::id& id() const { return id_; }
        wsrep::seqno seqno() const { return seqno_ ; }
        bool is_undefined() const
        {
            return (seqno_.is_undefined() && id_.is_undefined());
        }
        static wsrep::gtid undefined()
        {
            static const wsrep::gtid ret(wsrep::id::undefined(),
                                         wsrep::seqno::undefined());
            return ret;
        }
    private:
        wsrep::id id_;
        wsrep::seqno seqno_;
    };

    std::ostream& operator<<(std::ostream&, const wsrep::gtid&);
    std::istream& operator>>(std::istream&, wsrep::gtid&);
}

#endif // WSREP_GTID_HPP
