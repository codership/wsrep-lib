//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_APPLYING_SERVICE_HPP
#define WSREP_APPLYING_SERVICE_HPP

#include "transaction_termination_service.hpp"

namespace wsrep
{
    class ws_handle;
    class ws_meta;
    class applying_service : public wsrep::transaction_termination_service
    {
    public:
        virtual int start_transaction(const wsrep::ws_handle&,
                                      const wsrep::ws_meta&) = 0;
        virtual int apply_write_set(const wsrep::const_buffer&) = 0;
        virtual int apply_toi(const wsrep::const_buffer&);
        virtual int apply_nbo(const wsrep::const_buffer&);
    };
}

#endif // WSREP_APPLYING_SERVICE_HPP
