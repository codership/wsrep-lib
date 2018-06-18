//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction.hpp"
#include "mock_client_state.hpp"


int wsrep::mock_client_service::apply(
    const wsrep::const_buffer&)

{
    assert(client_state_.transaction().state() == wsrep::transaction::s_executing ||
           client_state_.transaction().state() == wsrep::transaction::s_replaying);
    return (fail_next_applying_ ? 1 : 0);
}

int wsrep::mock_client_service::commit(
    const wsrep::ws_handle&, const wsrep::ws_meta&)
{
    int ret(0);
    if (do_2pc())
    {
        if (client_state_.before_prepare())
        {
            ret = 1;
        }
        else if (client_state_.after_prepare())
        {
            ret = 1;
        }
    }
    if (ret == 0 &&
        (client_state_.before_commit() ||
         client_state_.ordered_commit() ||
         client_state_.after_commit()))
    {
        ret = 1;
    }
    return ret;
}

int wsrep::mock_client_service::rollback()
{
    int ret(0);
    if (client_state_.before_rollback())
    {
        ret = 1;
    }
    else if (client_state_.after_rollback())
    {
        ret = 1;
    }
    return ret;
}
