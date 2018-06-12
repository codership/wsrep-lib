//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_GTID_HPP
#define WSREP_GTID_HPP

#include "id.hpp"
#include "seqno.hpp"

namespace wsrep
{
    class gtid
    {
    public:
        gtid()
            : id_()
            , seqno_()
        { }
        gtid(const wsrep::id& id, wsrep::seqno seqno)
            : id_(id)
            , seqno_(seqno)
        { }
        const wsrep::id& id() const { return id_; }
        wsrep::seqno seqno() const { return seqno_ ; }
    private:
        wsrep::id id_;
        wsrep::seqno seqno_;
    };
}

#endif // WSREP_GTID_HPP
