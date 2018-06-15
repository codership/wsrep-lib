//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_server_context.hpp"
#include "db_server.hpp"

wsrep::client_context* db::server_context::local_client_context()
{
    return server_.local_client_context();
}

wsrep::client_context* db::server_context::streaming_applier_client_context()
{
    return server_.streaming_applier_client_context();
}

void db::server_context::release_client_context(
    wsrep::client_context* client_context)
{
    server_.release_client_context(client_context);
}

bool db::server_context::sst_before_init() const
{
    return false;
}

std::string db::server_context::on_sst_required()
{
    return id();
}

void db::server_context::on_sst_request(
    const std::string& request, const wsrep::gtid& gtid, bool bypass)
{
    server_.donate_sst(request, gtid, bypass);
}

void db::server_context::background_rollback(wsrep::client_context&)
{
}

void db::server_context::log_dummy_write_set(
    wsrep::client_context&, const wsrep::ws_meta& meta)
{
    wsrep::log_info() << "Dummy write set: " << meta.seqno();
}
