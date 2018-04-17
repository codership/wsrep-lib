//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "server_context.hpp"
#include "client_context.hpp"

#include <wsrep_api.h>

#define START_TRANSACTION(flags_)    ((flags_) & WSREP_FLAG_TRX_START)
#define COMMIT_TRANSACTION(flags_)   ((flags_) & WSREP_FLAG_TRX_END)
#define ROLLBACK_TRANSACTION(flags_) ((flags_) & WSREP_FLAG_ROLLBACK)

#if 0

namespace
{
    static wsrep_cb_status_t apply_cb(void* ctx,
                                      const wsrep_ws_handle_t* wsh,
                                      uint32_t flags,
                                      const wsrep_buf_t* buf,
                                      const wsrep_trx_meta_t* meta,
                                      wsrep_bool_t* exit_loop)
    {
        wsrep_cb_status_t ret(WSREP_CB_SUCCESS);

        trrep::client_context* client_context(
            reinterpret_cast<trrep::client_context*>(ctx));
        assert(client_context);
        assert(client_context.mode() == trrep::client_context::m_applier);

        const trrep::server_context& server_context(
            client_context->server_context());
        trrep::data data(buf->ptr, buf->len);

        if (START_TRANSACTION(flags) && COMMIT_TRANSACTION(flags))
        {
            trrep::transaction_context transasction_context(
                server_context.provider()
                *client_context);
            assert(transaction_context->active() == false);
            transaction_context->start_transaction(meta->stid.trx);
            if (client_context.apply(transaction_context, data))
            {
                ret = WSREP_CB_FAILURE;
            }
            else if (client_context.commit(transaction_context))
            {
                ret = WSREP_CB_FAILURE;
            }
        }

        else if (START_TRANSACTION(flags))
        {
            // First fragment of SR transaction
            trrep::client_context* sr_client_context(
                server_context.client_context(node_id, client_id, trx_id));
            assert(sr_client_context->transaction_context().active() == false);
        }
        else if (COMMIT_TRANSACTION(flags))
        {
            // Final fragment of SR transaction
            trrep::client_context* sr_client_context(
                server_context.client_context(node_id, client_id, trx_id));
            assert(sr_client_context->transaction_context().active());
            if (data.size() > 0)
            {
                sr_client_context->apply(data);
            }
            sr_client_context->commit();
        }
        else
        {
            // Middle fragment of SR transaction
            trrep::client_context* sr_client_context(
                server_context.client_context(node_id, client_id, trx_id));
            assert(sr_client_context->transaction_context().active());
            sr_client_context->apply(data);
        }

    }
    return ret;
}
#endif // 0

trrep::client_context* trrep::server_context::local_client_context()
{
    return new trrep::client_context(*this, ++client_id_,
                                     trrep::client_context::m_local);
}
