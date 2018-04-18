//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

//
// This file implementes a simple DBMS simulator which
// will launch one or server threads and replicates transactions
// through trrep interface.
//

#include "server_context.hpp"
#include "client_context.hpp"

#include <boost/program_options.hpp>

#include <iostream>
#include <memory>
#include <map>
#include <atomic>

class dbms_server;

class dbms_simulator
{
public:
    dbms_simulator(size_t n_servers,
                   size_t n_clients,
                   const std::string& wsrep_provider,
                   const std::string& wsrep_provider_options)
        : servers_()
        , n_servers_(n_servers)
        , n_clients_(n_clients)
        , wsrep_provider_(wsrep_provider)
        , wsrep_provider_options_(wsrep_provider_options)
    { }
    void start();
    void stop();
private:
    std::map<size_t, std::unique_ptr<dbms_server>> servers_;
    size_t n_servers_;
    size_t n_clients_;
    std::string wsrep_provider_;
    std::string wsrep_provider_options_;
};

class dbms_client;

class dbms_server : public trrep::server_context
{
public:
    dbms_server(const std::string& name, const std::string& id)
        : trrep::server_context(name, id, trrep::server_context::rm_async)
        , mutex_()
        , last_client_id_(0)
        , clients_()
    { }

    void on_connect() { }
    void on_view() { }
    void on_sync() { }
    trrep::client_context* local_client_context();
private:
    trrep::default_mutex mutex_;
    std::atomic<size_t> last_client_id_;
    std::map<size_t, std::unique_ptr<dbms_client>> clients_;
};

class dbms_client : public trrep::client_context
{
public:
    dbms_client(dbms_server& server, const trrep::client_id& id,
                enum trrep::client_context::mode mode)
        : trrep::client_context(mutex_, server, id, mode)
        , mutex_()
    { }
    bool do_2pc() const { return false; }
    int apply(trrep::transaction_context&, const trrep::data&)
    {
        return 0;
    }
    int commit(trrep::transaction_context&)
    {
        return 0;
    }
    int rollback(trrep::transaction_context&)
    {
        return 0;
    }
private:
    trrep::default_mutex mutex_;
};

trrep::client_context* dbms_server::local_client_context()
{
    std::ostringstream id_os;
    size_t client_id(last_client_id_.fetch_add(1) + 1);
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    return clients_.insert(std::make_pair(client_id,
                                          std::make_unique<dbms_client>(
                                              *this, client_id,
                                              trrep::client_context::m_replicating))).first->second.get();
}


void dbms_simulator::start()
{
    std::cout << "Provider: " << wsrep_provider_ << "\n";
    for (size_t i(0); i < n_servers_; ++i)
    {
        std::ostringstream name_os;
        name_os << (i + 1);
        std::ostringstream id_os;
        id_os << (i + 1);
        auto it(servers_.insert(std::make_pair((i + 1),
                                               std::make_unique<dbms_server>(
                                                   name_os.str(), id_os.str()))));
        it.first->second->load_provider(wsrep_provider_, wsrep_provider_options_);
    }
}

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    try
    {
        size_t n_servers;
        size_t n_clients;
        std::string wsrep_provider;
        std::string wsrep_provider_options;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("wsrep-provider",
             po::value<std::string>(&wsrep_provider)->required(),
             "wsrep provider to load")
            ("wsrep-provider-options",
             po::value<std::string>(&wsrep_provider_options),
             "wsrep provider options")
            ("servers", po::value<size_t>(&n_servers)->required(),
             "number of servers to start")
            ("clients", po::value<size_t>(&n_clients)->required(),
             "number of clients to start per server");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << "\n";
            return 1;
        }

        dbms_simulator sim(n_servers, n_clients, wsrep_provider, wsrep_provider_options);
        sim.start();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
