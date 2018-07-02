//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_client_service.hpp"
#include "db_high_priority_service.hpp"
#include "db_client.hpp"


int db::client_service::commit(const wsrep::ws_handle&,
                               const wsrep::ws_meta&)
{
    db::client* client(client_state_.client());
    int ret(client_state_.before_commit());
    if (ret == 0) client->se_trx_.commit();
    ret = ret || client_state_.ordered_commit();
    ret = ret || client_state_.after_commit();
    return ret;
}

int db::client_service::rollback()
{
    db::client* client(client_state_.client());
    int ret(client_state_.before_rollback());
    assert(ret == 0);
    client->se_trx_.rollback();
    ret = client_state_.after_rollback();
    assert(ret == 0);
    return ret;
}

enum wsrep::provider::status
db::client_service::replay()
{
    wsrep::high_priority_context high_priority_context(client_state_);
    db::high_priority_service high_priority_service(
        client_state_.client()->server_, client_state_.client());
    auto ret(client_state_.provider().replay(
                 client_state_.transaction().ws_handle(),
                 &high_priority_service));
    if (ret == wsrep::provider::success)
    {
        ++client_state_.client()->stats_.replays;
    }
    return ret;
}
