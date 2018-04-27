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
#include "transaction_context.hpp"
#include "key.hpp"
#include "data.hpp"
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

struct dbms_simulator_params
{
    size_t n_servers;
    size_t n_clients;
    size_t n_transactions;
    std::string wsrep_provider;
    std::string wsrep_provider_options;
    dbms_simulator_params()
        : n_servers(0)
        , n_clients(0)
        , n_transactions(0)
        , wsrep_provider()
        , wsrep_provider_options()
    { }
};

class dbms_simulator
{
public:
    dbms_simulator(const dbms_simulator_params& params)
        : mutex_()
        , params_(params)
        , servers_()
        , clients_start_()
        , clients_stop_()
    { }
    ~dbms_simulator()
    {
    }
    void start();
    void stop();
    void donate_sst(dbms_server&,
                    const std::string& req, const wsrep_gtid_t& gtid, bool);
    const dbms_simulator_params& params() const
    { return params_; }
    std::string stats() const
    {
        size_t transactions(params_.n_servers * params_.n_clients
                            * params_.n_transactions);
        auto duration(std::chrono::duration<double>(
                          clients_stop_ - clients_start_).count());
        std::ostringstream os;
        os << "Number of transactions: " << transactions
           << "\n"
           << "Seconds: " << duration
           << " \n"
           << "Transactions per second: " << transactions/duration
           << "\n";
        return os.str();
    }
private:
    std::string server_port(size_t i) const
    {
        std::ostringstream os;
        os << (10000 + (i + 1)*10);
        return os.str();
    }
    std::string build_cluster_address() const;

    trrep::default_mutex mutex_;
    const dbms_simulator_params& params_;
    std::map<size_t, std::unique_ptr<dbms_server>> servers_;
    std::chrono::time_point<std::chrono::steady_clock> clients_start_;
        std::chrono::time_point<std::chrono::steady_clock> clients_stop_;
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
                const std::string& name,
                const std::string& id,
                const std::string& address)
        : trrep::server_context(mutex_,
                                cond_,
                                name, id, address, name + "_data",
                                trrep::server_context::rm_async)
        , simulator_(simulator)
        , mutex_()
        , cond_()
        , state_(s_disconnected)
        , last_client_id_(0)
        , last_transaction_id_(0)
        , appliers_()
        , clients_()
        , client_threads_()
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

    bool sst_before_init() const override  { return false; }
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

    // Client context management
    trrep::client_context* local_client_context();

    size_t next_transaction_id()
    {
        return (last_transaction_id_.fetch_add(1) + 1);
    }

    void start_clients();
    void stop_clients();
    void client_thread(const std::shared_ptr<dbms_client>& client);
private:

    void start_client(size_t id);

    dbms_simulator& simulator_;
    trrep::default_mutex mutex_;
    trrep::default_condition_variable cond_;
    enum state state_;
    std::atomic<size_t> last_client_id_;
    std::atomic<size_t> last_transaction_id_;
    std::vector<std::thread> appliers_;
    std::map<size_t, std::unique_ptr<dbms_client>> clients_;
    std::vector<std::thread> client_threads_;
};

class dbms_client : public trrep::client_context
{
public:
    dbms_client(dbms_server& server,
                const trrep::client_id& id,
                enum trrep::client_context::mode mode,
                size_t n_transactions)
        : trrep::client_context(mutex_, server, id, mode)
        , mutex_()
        , server_(server)
        , n_transactions_(n_transactions)
        , result_()
    { }

    ~dbms_client()
    {
        std::cout << "Result: " << result_;
    }
    void start()
    {
        for (size_t i(0); i < n_transactions_; ++i)
        {
            run_one_transaction();
            report_progress(i + 1);
        }
    }

private:
    bool do_2pc() const override { return false; }
    int apply(trrep::transaction_context&, const trrep::data&) override
    {
        // std::cerr << "applying" << "\n";
        return 0;
    }
    int commit(trrep::transaction_context& transaction_context) override
    {
        int ret(0);
        ret = transaction_context.before_commit();
        ret = ret || transaction_context.ordered_commit();
        ret = ret || transaction_context.after_commit();
        // std::cerr << "commit" << "\n";
        return 0;
    }
    int rollback(trrep::transaction_context& transaction_context) override
    {
        std::cerr << "rollback: " << transaction_context.id().get()
                  << "state: " << trrep::to_string(transaction_context.state())
                  << "\n";
        transaction_context.before_rollback();
        transaction_context.after_rollback();
        return 0;
    }

    void will_replay(trrep::transaction_context&) override { }
    int replay(trrep::unique_lock<trrep::mutex>& lock,
               trrep::transaction_context& tc) override
    {
        tc.state(lock, trrep::transaction_context::s_replaying);
        tc.state(lock, trrep::transaction_context::s_committed);
        return 0;
    }
    void wait_for_replayers(trrep::unique_lock<trrep::mutex>&) const override
    { }
    void override_error(const trrep::client_error&) override { }
    bool killed() const override { return false; }
    void abort() const override { ::abort(); }
    void store_globals() override { }
    void debug_sync(const std::string&) override { }
    void debug_suicide(const std::string&) override { }

    void run_one_transaction()
    {
        trrep::transaction_context trx(*this);
        trx.start_transaction(server_.next_transaction_id());
        std::ostringstream os;
        int err(0);
        for (int i(0); i < 1 && err == 0; ++i)
        {
            int data(std::rand() % 10000000);
            os << data;
            trrep::key key;
            key.append_key_part("dbms", 4);
            wsrep_conn_id_t client_id(id().get());
            key.append_key_part(&client_id, sizeof(client_id));
            key.append_key_part(&data, sizeof(data));
            err = trx.append_key(key);
            err = err || trx.append_data(trrep::data(os.str().c_str(), os.str().size()));
        }
        // std::cout << "append_data: " << err << "\n";
        if (do_2pc())
        {
            err = err || trx.before_prepare();
            // std::cout << "before_prepare: " << err << "\n";
            err = err || trx.after_prepare();
            // std::cout << "after_prepare: " << err << "\n";
        }
        err = err || trx.before_commit();
        // std::cout << "before_commit: " << err << "\n";
        err = err || trx.ordered_commit();
        // std::cout << "ordered_commit: " << err << "\n";
        err = err || trx.after_commit();
        // std::cout << "after_commit: " << err << "\n";
        trx.after_statement();
    }

    void report_progress(size_t i) const
    {
        if ((i % 100) == 0)
        {
            std::cout << "client: " << id().get()
                      << " transactions: " << i
                      << " " << 100*double(i)/n_transactions_ << "%"
                      << std::endl;
        }
    }
    trrep::default_mutex mutex_;
    dbms_server& server_;
    const size_t n_transactions_;
    size_t result_;
};


// Server methods
void dbms_server::applier_thread()
{
    trrep::client_id client_id(last_client_id_.fetch_add(1) + 1);
    dbms_client applier(*this, client_id,
                        trrep::client_context::m_applier, 0);
    wsrep_status_t ret(provider().run_applier(&applier));
    std::cerr << "Applier thread exited with error code " << ret << "\n";
}

trrep::client_context* dbms_server::local_client_context()
{
    std::ostringstream id_os;
    size_t client_id(++last_client_id_);
    return new dbms_client(*this, client_id, trrep::client_context::m_replicating, 0);
}

void dbms_server::start_clients()
{
    size_t n_clients(simulator_.params().n_clients);
    for (size_t i(0); i < n_clients; ++i)
    {
        start_client(i + 1);
    }
}

void dbms_server::stop_clients()
{
    for (auto& i : client_threads_)
    {
        i.join();
    }
}

void dbms_server::client_thread(const std::shared_ptr<dbms_client>& client)
{
    client->start();
}

void dbms_server::start_client(size_t id)
{
    auto client(std::make_shared<dbms_client>(
                    *this, id,
                    trrep::client_context::m_replicating,
                    simulator_.params().n_transactions));
    client_threads_.push_back(
        std::thread(&dbms_server::client_thread, this, client));
}



void dbms_simulator::start()
{
    std::cout << "Provider: " << params_.wsrep_provider << "\n";

    std::string cluster_address(build_cluster_address());
    std::cout << "Cluster address: " << cluster_address << "\n";
    for (size_t i(0); i < params_.n_servers; ++i)
    {
        std::ostringstream name_os;
        name_os << (i + 1);
        std::ostringstream id_os;
        id_os << (i + 1);
        std::ostringstream address_os;
        address_os << "127.0.0.1:" << server_port(i);
        auto it(servers_.insert(
                    std::make_pair(
                        (i + 1),
                        std::make_unique<dbms_server>(
                            *this,
                            name_os.str(),
                            id_os.str(),
                            address_os.str()))));
        if (it.second == false)
        {
            throw trrep::runtime_error("Failed to add server");
        }
        boost::filesystem::path dir(std::string("./") + id_os.str() + "_data");
        boost::filesystem::create_directory(dir);

        dbms_server& server(*it.first->second);
        std::string server_options(params_.wsrep_provider_options);
        // server_options += "; base_port=" + server_port(i);
        if (server.load_provider(params_.wsrep_provider, server_options))
        {
            throw trrep::runtime_error("Failed to load provider");
        }
        if (server.provider().connect("sim_cluster", cluster_address, "",
                                      i == 0))
        {
            throw trrep::runtime_error("Failed to connect");
        }
        server.start_applier();
        server.wait_until_state(trrep::server_context::s_synced);
    }

    // Start client threads
    std::cout << "####################### Starting client load" << "\n";
    clients_start_ = std::chrono::steady_clock::now();
    for (auto& i : servers_)
    {
        i.second->start_clients();
    }

}

void dbms_simulator::stop()
{
    for (auto& i : servers_)
    {
        dbms_server& server(*i.second);
        server.stop_clients();
    }
    clients_stop_ = std::chrono::steady_clock::now();
    std::cout << "######## Stats ############\n";
    std::cout << stats();
    std::cout << "######## Stats ############\n";
    // REMOVEME: Temporary shortcut
    exit(0);
    for (auto& i : servers_)
    {
        dbms_server& server(*i.second);
        std::cout << "Status for server: " << server.id() << "\n";
        auto status(server.provider().status());
        for_each(status.begin(), status.end(),
                 [](const trrep::provider::status_variable& sv)
                 {
                     std::cout << sv.name() << " = " << sv.value() << "\n";
                 });

        server.provider().disconnect();
        server.wait_until_state(trrep::server_context::s_disconnected);
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
    std::string ret;
    if (params_.wsrep_provider.find("galera_smm") != std::string::npos)
    {
        ret += "gcomm://";
    }

    for (size_t i(0); i < params_.n_servers; ++i)
    {
        std::ostringstream sa_os;
        sa_os << "127.0.0.1:";
        sa_os << server_port(i);
        ret += sa_os.str();
        if (i < params_.n_servers - 1) ret += ",";
    }
    return ret;
}

namespace po = boost::program_options;

int main(int argc, char** argv)
{
    std::string stats;
    try
    {
        dbms_simulator_params params;
        po::options_description desc("Allowed options");
        desc.add_options()
            ("help", "produce help message")
            ("wsrep-provider",
             po::value<std::string>(&params.wsrep_provider)->required(),
             "wsrep provider to load")
            ("wsrep-provider-options",
             po::value<std::string>(&params.wsrep_provider_options),
             "wsrep provider options")
            ("servers", po::value<size_t>(&params.n_servers)->required(),
             "number of servers to start")
            ("clients", po::value<size_t>(&params.n_clients)->required(),
             "number of clients to start per server")
            ("transactions", po::value<size_t>(&params.n_transactions),
             "number of transactions run by a client");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cout << desc << "\n";
            return 1;
        }

        dbms_simulator sim(params);
        try
        {
            sim.start();
            sim.stop();
        }
        catch (const std::exception& e)
        {
            std::cerr << "Caught exception: " << e.what();
        }
        stats = sim.stats();
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << "\n";
        return 1;
    }

    std::cout << "Stats:\n" << stats << "\n";

    return 0;
}
