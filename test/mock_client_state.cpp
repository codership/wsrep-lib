//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction.hpp"
#include "mock_client_state.hpp"
#include "mock_high_priority_service.hpp"

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

enum wsrep::provider::status
wsrep::mock_client_service::replay() WSREP_OVERRIDE
{
    wsrep::mock_high_priority_service hps(client_state_.server_state(),
                                          &client_state_, true);
    enum wsrep::provider::status ret(
        client_state_.provider().replay(
            client_state_.transaction().ws_handle(),
            &hps));
    ++replays_;
    return ret;
}
