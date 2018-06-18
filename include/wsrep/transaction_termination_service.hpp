//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//


#ifndef WSREP_TRANSACTION_TERMINATION_SERVICE_HPP
#define WSREP_TRANSACTION_TERMINATION_SERVICE_HPP

namespace wsrep
{
    class ws_handle;
    class ws_meta;
    class transaction_termination_service
    {
    public:
        virtual int commit(const wsrep::ws_handle&, const wsrep::ws_meta&) = 0;
        virtual int rollback() = 0;
    };
}

#endif // WSREP_TRANSACTION_TERMINATION_SERVICE_HPP
