//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_WSREP_PROVIDER_V26_HPP
#define TRREP_WSREP_PROVIDER_V26_HPP

#include "provider_impl.hpp"

#include <wsrep_api.h>

namespace trrep
{
    class wsrep_provider_v26 : public trrep::provider_impl
    {
    public:

        wsrep_provider_v26(struct wsrep_init_args*)
        { }
        int start_transaction(wsrep_ws_handle_t*) { return 0; }
        int append_key(wsrep_ws_handle_t*, const wsrep_key_t*) { return 0; }
        int append_data(wsrep_ws_handle_t*, const wsrep_buf_t*) { return 0; }
        wsrep_status_t
        certify(wsrep_conn_id_t, wsrep_ws_handle_t*,
                uint32_t,
                wsrep_trx_meta_t*) { return WSREP_OK; }
        int rollback(const wsrep_trx_id_t) { return 0; }
        wsrep_status commit_order_enter(wsrep_ws_handle_t*) { return WSREP_OK; }
        int commit_order_leave(wsrep_ws_handle_t*) { return 0; }
        int release(wsrep_ws_handle_t*) { return 0; }

    };
}


#endif // TRREP_WSREP_PROVIDER_V26_HPP
