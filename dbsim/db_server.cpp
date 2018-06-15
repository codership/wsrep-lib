//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_server.hpp"

void db::server::applier_thread()
{
    wsrep::client_id client_id(last_client_id_.fetch_add(1) + 1);
    db::client applier(*this, client_id,
                       wsrep::client_context::m_applier, simulator_.params());
    enum wsrep::provider::status ret(provider().run_applier(&applier));
    wsrep::log() << "Applier thread exited with error code " << ret;
}

wsrep::client_context* db::server::local_client_context()
{
    std::ostringstream id_os;
    size_t client_id(++last_client_id_);
    return new db::client(*this, client_id,
                          wsrep::client_context::m_replicating,
                          simulator_.params());
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
        struct db::client::stats stats(i->stats());
        simulator_.stats_.commits += stats.commits;
        simulator_.stats_.aborts  += stats.aborts;
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

