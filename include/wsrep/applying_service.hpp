//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_APPLYING_SERVICE_HPP
#define WSREP_APPLYING_SERVICE_HPP

namespace wsrep
{
    class applying_service
    {
    public:
        virtual int start_transaction();
        virtual int apply_write_set(const wsrep::const_buffer&) = 0;
        virtual int apply_toi(const wsrep::const_buffer&);
        virtual int apply_nbo(const wsrep::const_buffer&);
    };
}

#endif // WSREP_APPLYING_SERVICE_HPP
