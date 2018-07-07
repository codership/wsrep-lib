//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_client_service.hpp"
#include "db_high_priority_service.hpp"
#include "db_client.hpp"

db::client_service::client_service(db::client& client)
    : wsrep::client_service()
    , client_(client)
    , client_state_(client_.client_state())
{ }

bool db::client_service::do_2pc() const
{
    return client_.do_2pc();
}

int db::client_service::bf_rollback()
{
    int ret(client_state_.before_rollback());
    assert(ret == 0);
    client_.se_trx_.rollback();
    ret = client_state_.after_rollback();
    assert(ret == 0);
    return ret;
}

enum wsrep::provider::status
db::client_service::replay()
{
    wsrep::high_priority_context high_priority_context(client_state_);
    db::high_priority_service high_priority_service(
        client_.server_, client_);
    auto ret(client_state_.provider().replay(
                 client_state_.transaction().ws_handle(),
                 &high_priority_service));
    if (ret == wsrep::provider::success)
    {
        ++client_.stats_.replays;
    }
    return ret;
}
