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
#include "provider.hpp"
#include "condition_variable.hpp"
#include "view.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>

#include <iostream>
#include <memory>
#include <map>
#include <atomic>
#include <thread>

class dbms_server;

class dbms_simulator
{
public:
    dbms_simulator(size_t n_servers,
                   size_t n_clients,
                   const std::string& wsrep_provider,
                   const std::string& wsrep_provider_options)
        : mutex_()
        , servers_()
        , n_servers_(n_servers)
        , n_clients_(n_clients)
        , wsrep_provider_(wsrep_provider)
        , wsrep_provider_options_(wsrep_provider_options)
    { }
    void start();
    void stop();
    void donate_sst(dbms_server&,
                    const std::string& req, const wsrep_gtid_t& gtid, bool);
private:
    std::string server_port(size_t i) const
    {
        std::ostringstream os;
        os << (10000 + (i + 1)*10);
        return os.str();
    }
    std::string build_cluster_address() const;

    trrep::default_mutex mutex_;
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
    enum state
    {
        s_disconnected,
        s_connected,
        s_synced
    };
    dbms_server(dbms_simulator& simulator,
                const std::string& name, const std::string& id)
        : trrep::server_context(name, id, name + "_data",
                                trrep::server_context::rm_async)
        , simulator_(simulator)
        , mutex_()
        , cond_()
        , state_(s_disconnected)
        , last_client_id_(0)
        , appliers_()
        , clients_()
    { }

    // Provider management

    void applier_thread();

    void start_applier()
    {
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        appliers_.push_back(std::thread(&dbms_server::applier_thread, this));
    }

    void stop_applier()
    {
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        appliers_.front().join();
        appliers_.erase(appliers_.begin());
    }

    void on_connect()
    {
        std::cerr << "dbms_server: connected" << "\n";
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        state_ = s_connected;
        cond_.notify_all();
    }

    void on_view(const trrep::view& view)
    {
        std::cerr << "================================================\nView:\n"
                  << "id: " << view.id() << "\n"
                  << "status: " << view.status() << "\n"
                  << "own_index: " << view.own_index() << "\n"
                  << "final: " << view.final() << "\n"
                  << "members: \n";
        auto members(view.members());
        for (const auto& m : members)
        {
            std::cerr << "id: " << m.id() << " "
                      << "name: " << m.name() << "\n";

        }
        std::cerr << "=================================================\n";
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        if (view.final())
        {
            state_ = s_disconnected;
            cond_.notify_all();
        }
    }

    void on_sync()
    {
        std::cerr << "Synced with group" << "\n";
    }

    std::string on_sst_request()
    {
        return id();
    }

    void on_sst_donate_request(const std::string& req,
                               const wsrep_gtid_t& gtid,
                               bool bypass)
    {
        simulator_.donate_sst(*this, req, gtid, bypass);
    }

    void sst_sent(const wsrep_gtid_t& gtid)
    {
        provider().sst_sent(gtid, 0);
    }
    void sst_received(const wsrep_gtid_t& gtid)
    {
        provider().sst_received(gtid, 0);
    }

    void wait_until_connected()
    {
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        while (state_ != s_connected)
        {
            cond_.wait(lock);
        }
    }
    void wait_until_disconnected()
    {
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        while (state_ != s_disconnected)
        {
            cond_.wait(lock);
        }
    }

    // Client context management
    trrep::client_context* local_client_context();
private:
    dbms_simulator& simulator_;
    trrep::default_mutex mutex_;
    trrep::default_condition_variable cond_;
    enum state state_;
    std::atomic<size_t> last_client_id_;
    std::vector<std::thread> appliers_;
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


// Server methods
void dbms_server::applier_thread()
{
    dbms_client applier(*this, ++last_client_id_,
                        trrep::client_context::m_applier);
    wsrep_status_t ret(provider().run_applier(&applier));
    std::cerr << "Applier thread exited with error code " << ret << "\n";
}

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

    std::string cluster_address(build_cluster_address());
    std::cout << "Cluster address: " << cluster_address << "\n";
    for (size_t i(0); i < n_servers_; ++i)
    {
        std::ostringstream name_os;
        name_os << (i + 1);
        std::ostringstream id_os;
        id_os << (i + 1);
        auto it(servers_.insert(std::make_pair((i + 1),
                                               std::make_unique<dbms_server>(
                                                   *this, name_os.str(), id_os.str()))));
        if (it.second == false)
        {
            throw trrep::runtime_error("Failed to add server");
        }
        boost::filesystem::path dir(std::string("./") + id_os.str() + "_data");
        boost::filesystem::create_directory(dir);

        dbms_server& server(*it.first->second);
        std::string server_options(wsrep_provider_options_);
        server_options += "; base_port=" + server_port(i);
        server.load_provider(wsrep_provider_, server_options);
        server.provider().connect("sim_cluster", cluster_address, "",
                                  i == 0);
        server.start_applier();
        server.wait_until_connected();
    }
}

void dbms_simulator::stop()
{
    for (auto& i : servers_)
    {
        dbms_server& server(*i.second);
        server.provider().disconnect();
        server.wait_until_disconnected();
        server.stop_applier();
    }
}

void dbms_simulator::donate_sst(dbms_server& server,
                                const std::string& req,
                                const wsrep_gtid_t& gtid,
                                bool bypass)
{
    size_t id;
    std::istringstream is(req);
    is >> id;
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    auto i(servers_.find(id));
    if (i == servers_.end())
    {
        throw trrep::runtime_error("Server " + req + " not found");
    }
    if (bypass == false)
    {
        std::cout << "SST " << server.id() << " -> " << id << "\n";
    }
    i->second->sst_received(gtid);
    server.sst_sent(gtid);
}
std::string dbms_simulator::build_cluster_address() const
{
    std::string ret("gcomm://");
    for (size_t i(0); i < n_servers_; ++i)
    {
        std::ostringstream sa_os;
        sa_os << "127.0.0.1:";
        sa_os << server_port(i);
        ret += sa_os.str();
        if (i < n_servers_ - 1) ret += ",";
    }
    return ret;
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
        std::this_thread::sleep_for(std::chrono::seconds(5));
        sim.stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }

    return 0;
}
