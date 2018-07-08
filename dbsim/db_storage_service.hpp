//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_STORAGE_SERVICE_HPP
#define WSREP_DB_STORAGE_SERVICE_HPP

#include "wsrep/storage_service.hpp"
#include "wsrep/exception.hpp"

namespace db
{
    class storage_service : public wsrep::storage_service
    {
        int start_transaction(const wsrep::ws_handle&) override
        { throw wsrep::not_implemented_error(); }
        int append_fragment(const wsrep::id&,
                            wsrep::transaction_id,
                            int,
                            const wsrep::const_buffer&) override
        { throw wsrep::not_implemented_error(); }
        int update_fragment_meta(const wsrep::ws_meta&) override
        { throw wsrep::not_implemented_error(); }
        int commit(const wsrep::ws_handle&, const wsrep::ws_meta&) override
        { throw wsrep::not_implemented_error(); }
        int rollback(const wsrep::ws_handle&, const wsrep::ws_meta&)
            override
        { throw wsrep::not_implemented_error(); }
        void store_globals() override { }
        void reset_globals() override { }
    };
}

#endif // WSREP_DB_STORAGE_SERVICE_HPP
