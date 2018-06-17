//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction.hpp"
#include "mock_client_state.hpp"


int wsrep::mock_client_service::apply(
    wsrep::client_state& client_state WSREP_UNUSED,
    const wsrep::const_buffer&)

{
    assert(client_state.transaction().state() == wsrep::transaction::s_executing ||
           client_state.transaction().state() == wsrep::transaction::s_replaying);
    return (fail_next_applying_ ? 1 : 0);
}

int wsrep::mock_client_service::commit(wsrep::client_state& client_state, const wsrep::ws_handle&, const wsrep::ws_meta&)
{
    int ret(0);
    if (do_2pc())
    {
        if (client_state.before_prepare())
        {
            ret = 1;
        }
        else if (client_state.after_prepare())
        {
            ret = 1;
        }
    }
    if (ret == 0 &&
        (client_state.before_commit() ||
         client_state.ordered_commit() ||
         client_state.after_commit()))
    {
        ret = 1;
    }
    return ret;
}

int wsrep::mock_client_service::rollback(
    wsrep::client_state& client_state)
{
    int ret(0);
    if (client_state.before_rollback())
    {
        ret = 1;
    }
    else if (client_state.after_rollback())
    {
        ret = 1;
    }
    return ret;
}
