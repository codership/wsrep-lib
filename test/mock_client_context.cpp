//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction_context.hpp"
#include "mock_client_context.hpp"


int wsrep::mock_client_service::apply(
    wsrep::client_context& client_context WSREP_UNUSED,
    const wsrep::const_buffer&)

{
    assert(client_context.transaction().state() == wsrep::transaction_context::s_executing ||
           client_context.transaction().state() == wsrep::transaction_context::s_replaying);
    return (fail_next_applying_ ? 1 : 0);
}

int wsrep::mock_client_service::commit(wsrep::client_context& client_context, const wsrep::ws_handle&, const wsrep::ws_meta&)
{
    int ret(0);
    if (do_2pc())
    {
        if (client_context.before_prepare())
        {
            ret = 1;
        }
        else if (client_context.after_prepare())
        {
            ret = 1;
        }
    }
    if (ret == 0 &&
        (client_context.before_commit() ||
         client_context.ordered_commit() ||
         client_context.after_commit()))
    {
        ret = 1;
    }
    return ret;
}

int wsrep::mock_client_service::rollback(
    wsrep::client_context& client_context)
{
    int ret(0);
    if (client_context.before_rollback())
    {
        ret = 1;
    }
    else if (client_context.after_rollback())
    {
        ret = 1;
    }
    return ret;
}
