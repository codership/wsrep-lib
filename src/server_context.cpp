//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "server_context.hpp"
#include "client_context.hpp"
#include "transaction_context.hpp"

// Todo: refactor into provider factory
#include "mock_provider.hpp"
#include "wsrep_provider_v26.hpp"

#include <wsrep_api.h>

#include <cassert>

namespace
{

    inline bool starts_transaction(uint32_t flags)
    {
        return (flags & WSREP_FLAG_TRX_START);
    }

    inline bool commits_transaction(uint32_t flags)
    {
        return (flags & WSREP_FLAG_TRX_END);
    }

    inline bool rolls_back_transaction(uint32_t flags)
    {
        return (flags & WSREP_FLAG_ROLLBACK);
    }

    static wsrep_cb_status_t apply_cb(void* ctx,
                                      const wsrep_ws_handle_t* wsh,
                                      uint32_t flags,
                                      const wsrep_buf_t* buf,
                                      const wsrep_trx_meta_t* meta,
                                      wsrep_bool_t* exit_loop __attribute__((unused)))
    {
        wsrep_cb_status_t ret(WSREP_CB_SUCCESS);

        trrep::client_context* client_context(
            reinterpret_cast<trrep::client_context*>(ctx));
        assert(client_context);
        assert(client_context->mode() == trrep::client_context::m_applier);

        trrep::data data(buf->ptr, buf->len);
        trrep::transaction_context transaction_context(*client_context,
                                                       *wsh,
                                                       *meta,
                                                       flags);
        if (client_context->server_context().on_apply(
                *client_context, transaction_context, data))
        {
            ret = WSREP_CB_FAILURE;
        }
        return ret;
    }
}

int trrep::server_context::load_provider(const std::string& provider_spec)
{
    if (provider_spec == "mock")
    {
        provider_ = new trrep::mock_provider;
    }
    else
    {
        struct wsrep_init_args init_args;
        init_args.apply_cb = &apply_cb;
        provider_ = new trrep::wsrep_provider_v26(&init_args);
    }
    return 0;
}

int trrep::server_context::on_apply(
    trrep::client_context& client_context,
    trrep::transaction_context& transaction_context,
    const trrep::data& data)
{
    int ret(0);
    if (starts_transaction(transaction_context.flags()) &&
        commits_transaction(transaction_context.flags()))
    {
        assert(transaction_context.active() == false);
        transaction_context.start_transaction();
        if (client_context.apply(transaction_context, data))
        {
            ret = 1;
        }
        else if (client_context.commit(transaction_context))
        {
            ret = 1;
        }
        assert(ret ||
               transaction_context.state() ==
               trrep::transaction_context::s_committed);
    }
    else
    {
        // SR not implemented yet
        assert(0);
    }

    if (ret)
    {
        client_context.rollback(transaction_context);
    }
    return ret;
}
