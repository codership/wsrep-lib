//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_PROVIDER_IMPL_HPP
#define TRREP_PROVIDER_IMPL_HPP

#include <wsrep_api.h>

namespace trrep
{
    // Abstract interface for provider implementations
    class provider_impl
    {
    public:
        virtual int start_transaction(wsrep_ws_handle_t*) = 0;
        virtual int append_key(wsrep_ws_handle_t*, const wsrep_key_t*) = 0;
        virtual int append_data(wsrep_ws_handle_t*, const wsrep_buf_t*) = 0;
        virtual wsrep_status_t
        certify(wsrep_conn_id_t, wsrep_ws_handle_t*,
                uint32_t,
                wsrep_trx_meta_t*) = 0;
        virtual int rollback(const wsrep_trx_id_t) = 0;
        virtual wsrep_status commit_order_enter(wsrep_ws_handle_t*) = 0;
        virtual int commit_order_leave(wsrep_ws_handle_t*) = 0;
        virtual int release(wsrep_ws_handle_t*) = 0;
    };
}


#endif // TRREP_PROVIDER_IMPL_HPP
