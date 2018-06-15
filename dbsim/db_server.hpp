//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_HPP
#define WSREP_DB_SERVER_HPP

namespace db
{
    class server
    {
public:
        server(simulator& simulator,
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

        storage_engine& storage_engine() { return storage_engine_; }

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

        simulator& simulator_;
        storage_engine storage_engine_;
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        std::atomic<size_t> last_client_id_;
        std::atomic<size_t> last_transaction_id_;
        std::vector<boost::thread> appliers_;
        std::vector<std::shared_ptr<dbms_client>> clients_;
        std::vector<boost::thread> client_threads_;
    };
};

#endif // WSREP_DB_SERVER_HPP
