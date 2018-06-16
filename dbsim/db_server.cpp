//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_server.hpp"
#include "db_client.hpp"
#include "db_simulator.hpp"

db::server::server(simulator& simulator,
                   const std::string& name,
                   const std::string& server_id,
                   const std::string& address)
    : simulator_(simulator)
    , storage_engine_(simulator_.params())
    , mutex_()
    , cond_()
    , server_context_(*this, name, server_id, address, "dbsim_" + name + "_data")
    , last_client_id_(0)
    , last_transaction_id_(0)
    , appliers_()
    , clients_()
    , client_threads_()
{ }

void db::server::applier_thread()
{
    wsrep::client_id client_id(last_client_id_.fetch_add(1) + 1);
    db::client applier(*this, client_id,
                       wsrep::client_context::m_high_priority,
                       simulator_.params());
    wsrep::client_context* cc(static_cast<wsrep::client_context*>(
                                  &applier.client_context()));
    enum wsrep::provider::status ret(
        server_context_.provider().run_applier(cc));
    wsrep::log() << "Applier thread exited with error code " << ret;
}

void db::server::start_applier()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    appliers_.push_back(boost::thread(&server::applier_thread, this));
}

void db::server::stop_applier()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    appliers_.front().join();
    appliers_.erase(appliers_.begin());
}


void db::server::start_clients()
{
    size_t n_clients(simulator_.params().n_clients);
    for (size_t i(0); i < n_clients; ++i)
    {
        start_client(i + 1);
    }
}

void db::server::stop_clients()
{
    for (auto& i : client_threads_)
    {
        i.join();
    }
    for (const auto& i : clients_)
    {
        const struct db::client::stats& stats(i->stats());
        simulator_.stats_.commits += stats.commits;
        simulator_.stats_.rollbacks  += stats.rollbacks;
        simulator_.stats_.replays += stats.replays;
    }
}

void db::server::client_thread(const std::shared_ptr<db::client>& client)
{
    client->store_globals();
    client->start();
}

void db::server::start_client(size_t id)
{
    auto client(std::make_shared<db::client>(
                    *this, id,
                    wsrep::client_context::m_replicating,
                    simulator_.params()));
    clients_.push_back(client);
    client_threads_.push_back(
        boost::thread(&db::server::client_thread, this, client));
}

void db::server::donate_sst(const std::string& req,
                            const wsrep::gtid& gtid,
                            bool bypass)
{
    simulator_.sst(*this, req, gtid, bypass);
}

wsrep::client_context* db::server::local_client_context()
{
    std::ostringstream id_os;
    size_t client_id(++last_client_id_);
    db::client* client(new db::client(*this, client_id,
                                      wsrep::client_context::m_replicating,
                                      simulator_.params()));
    return &client->client_context();
}

wsrep::client_context* db::server::streaming_applier_client_context()
{
    throw wsrep::not_implemented_error();
}

void db::server::release_client_context(wsrep::client_context* client_context)
{
    db::client_context* db_client_context(
        dynamic_cast<db::client_context*>(client_context));
    delete db_client_context->client();
}
