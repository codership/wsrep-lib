//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_server_state.hpp"
#include "db_server.hpp"

#include "wsrep/logger.hpp"

wsrep::client_state* db::server_state::local_client_state()
{
    return server_.local_client_state();
}

wsrep::client_state* db::server_state::streaming_applier_client_state()
{
    return server_.streaming_applier_client_state();
}

void db::server_state::release_client_state(
    wsrep::client_state* client_state)
{
    server_.release_client_state(client_state);
}

bool db::server_state::sst_before_init() const
{
    return false;
}

std::string db::server_state::on_sst_required()
{
    return id();
}

void db::server_state::on_sst_request(
    const std::string& request, const wsrep::gtid& gtid, bool bypass)
{
    server_.donate_sst(request, gtid, bypass);
}

void db::server_state::background_rollback(wsrep::client_state&)
{
}

void db::server_state::log_dummy_write_set(
    wsrep::client_state&, const wsrep::ws_meta& meta)
{
    wsrep::log_info() << "Dummy write set: " << meta.seqno();
}
