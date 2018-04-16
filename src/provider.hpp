//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_PROVIDER_HPP
#define TRREP_PROVIDER_HPP

#include "provider_impl.hpp"

#include <wsrep_api.h>

#include <string>

namespace trrep
{
    class provider
    {

    public:

        // Construct a provider with give provider implementation
        // 
        provider(provider_impl* impl) : impl_(impl) { }
        int start_transaction(wsrep_ws_handle_t* wsh)
        { return impl_->start_transaction(wsh); }
        int append_key(wsrep_ws_handle_t* wsh, const wsrep_key_t* key)
        { return impl_->append_key(wsh, key); }
        int append_data(wsrep_ws_handle_t* wsh, const wsrep_buf_t* buf)
        { return impl_->append_data(wsh, buf); }
        wsrep_status certify_commit(wsrep_conn_id_t conn_id,
                                    wsrep_ws_handle_t* wsh,
                                    uint32_t flags, wsrep_trx_meta_t* trx_meta)
        { return impl_->certify_commit(conn_id, wsh, flags, trx_meta); }
        int rollback(const wsrep_trx_id_t trx_id)
        { return impl_->rollback(trx_id); }
        wsrep_status commit_order_enter(wsrep_ws_handle_t* wsh)
        { return impl_->commit_order_enter(wsh); }
        int commit_order_leave(wsrep_ws_handle_t* wsh)
        { return impl_->commit_order_leave(wsh); }
        int release(wsrep_ws_handle_t* wsh)
        { return impl_->release(wsh); }

        // Load the provider
        // @param Path to provider library or "none" to load
        //        dummy provider
        static provider* make_provider(const std::string& provider);
    private:
        provider_impl* impl_;
    };
}

#endif // TRREP_PROVIDER_HPP
