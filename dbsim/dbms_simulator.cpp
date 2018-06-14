//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

//
// This file implementes a simple DBMS simulator which
// will launch one or server threads and replicates transactions
// through wsrep interface.
//


#include "wsrep/server_context.hpp"
#include "wsrep/client_context.hpp"
#include "wsrep/transaction_context.hpp"
#include "wsrep/key.hpp"
#include "wsrep/provider.hpp"
#include "wsrep/condition_variable.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"

#include <boost/program_options.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

#include <memory>
#include <map>
#include <atomic>
#include <chrono>
#include <iostream>
#include <unordered_set>
#include <thread>

class dbms_server;

struct dbms_simulator_params
{
    size_t n_servers;
    size_t n_clients;
    size_t n_transactions;
    size_t n_rows;
    size_t alg_freq;
    std::string wsrep_provider;
    std::string wsrep_provider_options;
    int debug_log_level;
    int fast_exit;
    dbms_simulator_params()
        : n_servers(0)
        , n_clients(0)
        , n_transactions(0)
        , n_rows(1000)
        , alg_freq(0)
        , wsrep_provider()
        , wsrep_provider_options()
        , debug_log_level(0)
        , fast_exit(0)
    { }
};

class dbms_storage_engine
{
public:
    dbms_storage_engine(const dbms_simulator_params& params)
        : mutex_()
        , transactions_()
        , alg_freq_(params.alg_freq)
        , bf_aborts_()
    { }

    class transaction
    {
    public:
        transaction(dbms_storage_engine& se)
            : se_(se)
            , cc_()
        {
        }

        bool active() const { return cc_ != nullptr; }

        void start(wsrep::client_context* cc)
        {
            // wsrep::log_debug() << "Start: " << cc;
            wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
            if (se_.transactions_.insert(cc).second == false)
            {
                ::abort();
            }
            cc_ = cc;
        }

        void commit()
        {
            // wsrep::log_debug() << "Commit: " << cc_;
            if (cc_)
            {
                wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
                se_.transactions_.erase(cc_);
            }
            cc_ = nullptr;
        }


        void abort()
        {
            // wsrep::log_debug() << "Abort: " << cc_;
            if (cc_)
            {
                wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
                se_.transactions_.erase(cc_);
            }
            cc_ = nullptr;
        }

        ~transaction()
        {
            abort();
        }

        transaction(const transaction&) = delete;
        transaction& operator=(const transaction&) = delete;

    private:
        dbms_storage_engine& se_;
        wsrep::client_context* cc_;
    };

    void bf_abort_some(const wsrep::transaction_context& txc)
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        if (alg_freq_ && (std::rand() % alg_freq_) == 0)
        {
            if (transactions_.empty() == false)
            {
                auto* victim_txc(*transactions_.begin());
                wsrep::unique_lock<wsrep::mutex> victim_txc_lock(
                    victim_txc->mutex());
                lock.unlock();
                if (victim_txc->bf_abort(victim_txc_lock, txc.seqno()))
                {
                    // wsrep::log() << "BF aborted " << victim_txc->id().get();
                    ++bf_aborts_;
                }
            }
        }
    }

    long long bf_aborts()
    {
        return bf_aborts_;
    }
private:
    wsrep::default_mutex mutex_;
    std::unordered_set<wsrep::client_context*> transactions_;
    size_t alg_freq_;
    std::atomic<long long> bf_aborts_;
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
        , stats_()
    { }
    ~dbms_simulator()
    {
    }
    void start();
    void stop();
    void donate_sst(dbms_server&,
                    const std::string& req, const wsrep::gtid& gtid, bool);
    const dbms_simulator_params& params() const
    { return params_; }
    std::string stats() const;
private:
    std::string server_port(size_t i) const
    {
        std::ostringstream os;
        os << (10000 + (i + 1)*10);
        return os.str();
    }
    std::string build_cluster_address() const;

    wsrep::default_mutex mutex_;
    const dbms_simulator_params& params_;
    std::map<size_t, std::unique_ptr<dbms_server>> servers_;
    std::chrono::time_point<std::chrono::steady_clock> clients_start_;
    std::chrono::time_point<std::chrono::steady_clock> clients_stop_;
public:
    struct stats
    {
        long long commits;
        long long aborts;
        long long replays;
        stats()
            : commits(0)
            , aborts(0)
            , replays(0)
        { }
    } stats_;
};

class dbms_client;

class dbms_server : public wsrep::server_context
{
public:
    dbms_server(dbms_simulator& simulator,
                const std::string& name,
                const std::string& id,
                const std::string& address)
        : wsrep::server_context(mutex_,
                                cond_,
                                name, id, address, name + "_data",
                                wsrep::server_context::rm_async)
        , simulator_(simulator)
        , storage_engine_(simulator_.params())
        , mutex_()
        , cond_()
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
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        appliers_.push_back(boost::thread(&dbms_server::applier_thread, this));
    }

    void stop_applier()
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        appliers_.front().join();
        appliers_.erase(appliers_.begin());
    }

    bool sst_before_init() const override  { return false; }
    std::string on_sst_required()
    {
        return id();
    }

    void on_sst_request(const std::string& req,
                        const wsrep::gtid& gtid,
                        bool bypass)
    {
        simulator_.donate_sst(*this, req, gtid, bypass);
    }
    void background_rollback(wsrep::client_context& cs) override
    {
        assert(0);
        cs.before_rollback();
        cs.after_rollback();
    }

    // Client context management
    wsrep::client_context* local_client_context();

    wsrep::client_context* streaming_applier_client_context() override
    {
        throw wsrep::not_implemented_error();
    }
    void log_dummy_write_set(wsrep::client_context&, const wsrep::ws_meta&)
        override
    { }
    size_t next_transaction_id()
    {
        return (last_transaction_id_.fetch_add(1) + 1);
    }

    dbms_storage_engine& storage_engine() { return storage_engine_; }

    int apply_to_storage_engine(const wsrep::transaction_context& txc,
                                const wsrep::const_buffer&)
    {
        storage_engine_.bf_abort_some(txc);
        return 0;
    }

    void start_clients();
    void stop_clients();
    void client_thread(const std::shared_ptr<dbms_client>& client);
private:

    void start_client(size_t id);

    dbms_simulator& simulator_;
    dbms_storage_engine storage_engine_;
    wsrep::default_mutex mutex_;
    wsrep::default_condition_variable cond_;
    std::atomic<size_t> last_client_id_;
    std::atomic<size_t> last_transaction_id_;
    std::vector<boost::thread> appliers_;
    std::vector<std::shared_ptr<dbms_client>> clients_;
    std::vector<boost::thread> client_threads_;
};

class dbms_client : public wsrep::client_context
{
public:
    dbms_client(dbms_server& server,
                const wsrep::client_id& id,
                enum wsrep::client_context::mode mode,
                const dbms_simulator_params& params)
        : wsrep::client_context(mutex_, server, id, mode)
        , mutex_()
        , server_(server)
        , se_trx_(server_.storage_engine())
        , params_(params)
        , stats_()
    { }

    ~dbms_client()
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    }

    void start()
    {
        for (size_t i(0); i < params_.n_transactions; ++i)
        {
            run_one_transaction();
            report_progress(i + 1);
        }
    }

    struct stats
    {
        long long commits;
        long long aborts;
        long long replays;
        stats()
            : commits(0)
            , aborts(0)
            , replays(0)
        { }
    };

    const struct stats stats() const
    {
        return stats_;
    }
private:
    bool is_autocommit() const override { return false; }
    bool do_2pc() const override { return false; }
    int append_fragment(const wsrep::transaction_context&,
                        int, const wsrep::const_buffer&) override
    {
        return 0;
    }
    void remove_fragments(const wsrep::transaction_context&) override
    {
    }
    int apply(const wsrep::const_buffer& data) override
    {
        return server_.apply_to_storage_engine(transaction(), data);
    }
    int commit() override
    {
        assert(mode() == wsrep::client_context::m_applier);
        int ret(0);
        ret = before_commit();
        se_trx_.commit();
        ret = ret || ordered_commit();
        ret = ret || after_commit();
        return ret;
    }
    int rollback() override
    {
        // wsrep::log() << "rollback: " << transaction().id().get()
        //             << "state: "
        //            << wsrep::to_string(transaction().state());
        before_rollback();
        se_trx_.abort();
        after_rollback();
        return 0;
    }

    void will_replay(wsrep::transaction_context&) override { }
    int replay(wsrep::transaction_context& txc) override
    {
        // wsrep::log() << "replay: " << txc.id().get();
        wsrep::client_applier_mode applier_mode(*this);
        ++stats_.replays;
        return provider().replay(txc.ws_handle(), this);
    }
    void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>&) override
    { }
    size_t bytes_generated() const { return 0; }
    int prepare_data_for_replication(const wsrep::transaction_context&)
        override
    { return 0; }
    int prepare_fragment_for_replication(const wsrep::transaction_context&,
                                          wsrep::mutable_buffer&) override
    { return 0; }
    bool killed() const override { return false; }
    void abort() override { ::abort(); }
public:
    void store_globals() override
    {
        wsrep::client_context::store_globals();
    }
private:
    void debug_sync(wsrep::unique_lock<wsrep::mutex>&, const char*) override { }
    void debug_suicide(const char*) override { }
    void on_error(enum wsrep::client_error) override { }

    template <class Func>
    int client_command(Func f)
    {
        int err(before_command());
        // wsrep::log_debug() << "before_command: " << err;
        // If err != 0, transaction was BF aborted while client idle
        if (err == 0)
        {
            err = before_statement();
            if (err == 0)
            {
                err = f();
            }
            after_statement();
        }
        after_command_before_result();
        if (current_error())
        {
            // wsrep::log_info() << "Current error";
            assert(transaction_.state() ==
                   wsrep::transaction_context::s_aborted);
            err = 1;
        }
        after_command_after_result();
        // wsrep::log_info() << "client_command(): " << err;
        return err;
    }

    void run_one_transaction()
    {
        reset_error();
        int err = client_command(
            [&]()
            {
                // wsrep::log_debug() << "Start transaction";
                err = start_transaction(server_.next_transaction_id());
                assert(err == 0);
                se_trx_.start(this);
                return err;
            });
        err = err || client_command(
            [&]()
            {
                // wsrep::log_debug() << "Generate write set";
                assert(transaction().active());
                assert(err == 0);
                int data(std::rand() % params_.n_rows);
                std::ostringstream os;
                os << data;
                wsrep::key key(wsrep::key::exclusive);
                key.append_key_part("dbms", 4);
                unsigned long long client_key(id().get());
                key.append_key_part(&client_key, sizeof(client_key));
                key.append_key_part(&data, sizeof(data));
                err = append_key(key);
                err = err || append_data(
                    wsrep::const_buffer(os.str().c_str(),
                                        os.str().size()));
                return err;
            });
        err = err || client_command(
            [&]()
            {
                // wsrep::log_debug() << "Commit";
                assert(err == 0);
                if (do_2pc())
                {
                    err = err || before_prepare();
                    err = err || after_prepare();
                }
                err = err || before_commit();
                if (err == 0) se_trx_.commit();
                err = err || ordered_commit();
                err = err || after_commit();
                return err;
            });

        assert(err ||
               transaction().state() == wsrep::transaction_context::s_aborted ||
               transaction().state() == wsrep::transaction_context::s_committed);
        assert(se_trx_.active() == false);
        assert(transaction().active() == false);
        switch (transaction().state())
        {
        case wsrep::transaction_context::s_committed:
            ++stats_.commits;
            break;
        case wsrep::transaction_context::s_aborted:
            ++stats_.aborts;
            break;
        default:
            assert(0);
        }
    }

    void report_progress(size_t i) const
    {
        if ((i % 1000) == 0)
        {
            wsrep::log() << "client: " << id().get()
                         << " transactions: " << i
                         << " " << 100*double(i)/params_.n_transactions << "%";
        }
    }
    wsrep::default_mutex mutex_;
    dbms_server& server_;
    dbms_storage_engine::transaction se_trx_;
    const dbms_simulator_params params_;
    struct stats stats_;
};


// Server methods
void dbms_server::applier_thread()
{
    wsrep::client_id client_id(last_client_id_.fetch_add(1) + 1);
    dbms_client applier(*this, client_id,
                        wsrep::client_context::m_applier, simulator_.params());
    enum wsrep::provider::status ret(provider().run_applier(&applier));
    wsrep::log() << "Applier thread exited with error code " << ret;
}

wsrep::client_context* dbms_server::local_client_context()
{
    std::ostringstream id_os;
    size_t client_id(++last_client_id_);
    return new dbms_client(*this, client_id,
                           wsrep::client_context::m_replicating,
                           simulator_.params());
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
    for (const auto& i : clients_)
    {
        struct dbms_client::stats stats(i->stats());
        simulator_.stats_.commits += stats.commits;
        simulator_.stats_.aborts  += stats.aborts;
        simulator_.stats_.replays += stats.replays;
    }
}

void dbms_server::client_thread(const std::shared_ptr<dbms_client>& client)
{
    client->store_globals();
    client->start();
}

void dbms_server::start_client(size_t id)
{
    auto client(std::make_shared<dbms_client>(
                    *this, id,
                    wsrep::client_context::m_replicating,
                    simulator_.params()));
    clients_.push_back(client);
    client_threads_.push_back(
        boost::thread(&dbms_server::client_thread, this, client));
}



void dbms_simulator::start()
{
    wsrep::log() << "Provider: " << params_.wsrep_provider;

    std::string cluster_address(build_cluster_address());
    wsrep::log() << "Cluster address: " << cluster_address;
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
            throw wsrep::runtime_error("Failed to add server");
        }
        boost::filesystem::path dir(std::string("./") + id_os.str() + "_data");
        boost::filesystem::create_directory(dir);

        dbms_server& server(*it.first->second);
        server.debug_log_level(params_.debug_log_level);
        std::string server_options(params_.wsrep_provider_options);

        if (server.load_provider(params_.wsrep_provider, server_options))
        {
            throw wsrep::runtime_error("Failed to load provider");
        }
        if (server.connect("sim_cluster", cluster_address, "",
                                      i == 0))
        {
            throw wsrep::runtime_error("Failed to connect");
        }
        server.start_applier();
        server.wait_until_state(wsrep::server_context::s_synced);
    }

    // Start client threads
    wsrep::log() << "####################### Starting client load";
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
    wsrep::log() << "######## Stats ############";
    wsrep::log()  << stats();
    wsrep::log() << "######## Stats ############";
    if (params_.fast_exit)
    {
        exit(0);
    }
    for (auto& i : servers_)
    {
        dbms_server& server(*i.second);
        wsrep::log() << "Status for server: " << server.id();
        auto status(server.provider().status());
        for_each(status.begin(), status.end(),
                 [](const wsrep::provider::status_variable& sv)
                 {
                     wsrep::log() << sv.name() << " = " << sv.value();
                 });

        server.disconnect();
        server.wait_until_state(wsrep::server_context::s_disconnected);
        server.stop_applier();
    }
}

std::string dbms_simulator::stats() const
{
    size_t transactions(params_.n_servers * params_.n_clients
                        * params_.n_transactions);
    auto duration(std::chrono::duration<double>(
                      clients_stop_ - clients_start_).count());
    long long bf_aborts(0);
    for (const auto& s : servers_)
    {
        bf_aborts += s.second->storage_engine().bf_aborts();
    }
    std::ostringstream os;
    os << "Number of transactions: " << transactions
       << "\n"
       << "Seconds: " << duration
       << " \n"
       << "Transactions per second: " << transactions/duration
       << "\n"
       << "BF aborts: "
       << bf_aborts
       << "\n"
       << "Client commits: " << stats_.commits
       << "\n"
       << "Client aborts: " << stats_.aborts
       << "\n"
       << "Client replays: " << stats_.replays;
    return os.str();
}

void dbms_simulator::donate_sst(dbms_server& server,
                                const std::string& req,
                                const wsrep::gtid& gtid,
                                bool bypass)
{
    size_t id;
    std::istringstream is(req);
    is >> id;
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    auto i(servers_.find(id));
    if (i == servers_.end())
    {
        throw wsrep::runtime_error("Server " + req + " not found");
    }
    if (bypass == false)
    {
        wsrep::log() << "SST " << server.id() << " -> " << id;
    }
    i->second->sst_received(gtid, 0);
    server.sst_sent(gtid, 0);
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
             "number of transactions run by a client")
            ("rows", po::value<size_t>(&params.n_rows),
             "number of rows per table")
            ("alg-freq", po::value<size_t>(&params.alg_freq),
             "ALG frequency")
            ("debug-log-level", po::value<int>(&params.debug_log_level),
             "debug logging level: 0 - none, 1 - verbose")
            ("fast-exit", po::value<int>(&params.fast_exit),
             "exit from simulation without graceful shutdown");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help"))
        {
            std::cerr << desc << "\n";
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
            wsrep::log() << "Caught exception: " << e.what();
        }
        stats = sim.stats();
    }
    catch (const std::exception& e)
    {
        wsrep::log() << e.what();
        return 1;
    }

    wsrep::log() << "Stats:\n" << stats << "\n";

    return 0;
}
