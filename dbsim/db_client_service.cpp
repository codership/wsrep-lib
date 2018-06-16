//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_client_service.hpp"
#include "db_client.hpp"

int db::client_service::apply(wsrep::client_context&,
                              const wsrep::const_buffer&)
{
    db::client* client(client_context_.client());
    client->se_trx_.start(client);
    client->se_trx_.apply(client_context_.transaction());
    return 0;
}

int db::client_service::commit(wsrep::client_context&,
                               const wsrep::ws_handle&,
                               const wsrep::ws_meta&)
{
    db::client* client(client_context_.client());
    int ret(client_context_.before_commit());
    if (ret == 0) client->se_trx_.commit();
    ret = ret || client_context_.ordered_commit();
    ret = ret || client_context_.after_commit();
    return ret;
}

int db::client_service::rollback(wsrep::client_context&)
{
    db::client* client(client_context_.client());
    int ret(client_context_.before_rollback());
    assert(ret == 0);
    client->se_trx_.rollback();
    ret = client_context_.after_rollback();
    assert(ret == 0);
    return ret;
}
enum wsrep::provider::status
db::client_service::replay(wsrep::client_context&,
                           wsrep::transaction_context& transaction_context)
{
    wsrep::high_priority_context high_priority_context(client_context_);
    auto ret(provider_.replay(transaction_context.ws_handle(),
                              &client_context_));
    if (ret == wsrep::provider::success)
    {
        ++client_context_.client()->stats_.replays;
    }
    return ret;
}
