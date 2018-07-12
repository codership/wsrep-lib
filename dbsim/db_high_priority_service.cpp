//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_high_priority_service.hpp"
#include "db_server.hpp"
#include "db_client.hpp"

db::high_priority_service::high_priority_service(
    db::server& server, db::client& client)
    : wsrep::high_priority_service(server_.server_state())
    , server_(server)
    , client_(client)
{ }

int db::high_priority_service::start_transaction(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    return client_.client_state().start_transaction(ws_handle, ws_meta);
}

const wsrep::transaction& db::high_priority_service::transaction() const
{
    return client_.client_state().transaction();
}

void db::high_priority_service::adopt_transaction(const wsrep::transaction&)
{
    throw wsrep::not_implemented_error();
}

int db::high_priority_service::apply_write_set(
    const wsrep::ws_meta&,
    const wsrep::const_buffer&)
{
    client_.se_trx_.start(&client_);
    client_.se_trx_.apply(client_.client_state().transaction());
    return 0;
}

int db::high_priority_service::apply_toi(
    const wsrep::ws_meta&,
    const wsrep::const_buffer&)
{
    throw wsrep::not_implemented_error();
}

int db::high_priority_service::commit(const wsrep::ws_handle& ws_handle,
                                      const wsrep::ws_meta& ws_meta)
{
    client_.client_state_.prepare_for_ordering(ws_handle, ws_meta, true);
    int ret(client_.client_state_.before_commit());
    if (ret == 0) client_.se_trx_.commit();
    ret = ret || client_.client_state_.ordered_commit();
    ret = ret || client_.client_state_.after_commit();
    return ret;
}

int db::high_priority_service::rollback(const wsrep::ws_handle& ws_handle,
                                        const wsrep::ws_meta& ws_meta)
{
    client_.client_state_.prepare_for_ordering(ws_handle, ws_meta, false);
    int ret(client_.client_state_.before_rollback());
    assert(ret == 0);
    client_.se_trx_.rollback();
    ret = client_.client_state_.after_rollback();
    assert(ret == 0);
    return ret;
}

void db::high_priority_service::after_apply()
{
    client_.client_state_.after_statement();
}

bool db::high_priority_service::is_replaying() const
{
    return (client_.client_state_.transaction().state() == wsrep::transaction::s_replaying);
}
