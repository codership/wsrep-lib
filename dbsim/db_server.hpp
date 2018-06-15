//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_HPP
#define WSREP_DB_SERVER_HPP

#include "wsrep/gtid.hpp"
#include "wsrep/client_context.hpp"

#include "db_storage_engine.hpp"
#include "db_server_context.hpp"

#include <boost/thread.hpp>

#include <string>
#include <memory>

namespace db
{
    class simulator;
    class client;
    class server
    {
    public:
        server(simulator& simulator,
               const std::string& name,
               const std::string& id,
               const std::string& address);

        // Provider management

        void applier_thread();

        void start_applier();

        void stop_applier();



        db::storage_engine& storage_engine() { return storage_engine_; }

        int apply_to_storage_engine(const wsrep::transaction_context& txc,
                                    const wsrep::const_buffer&)
        {
            storage_engine_.bf_abort_some(txc);
            return 0;
        }

        void start_clients();
        void stop_clients();
        void client_thread(const std::shared_ptr<db::client>& client);
        db::server_context& server_context() { return server_context_; }
    private:

        void start_client(size_t id);

        db::simulator& simulator_;
        db::storage_engine storage_engine_;
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        db::server_context server_context_;
        std::atomic<size_t> last_client_id_;
        std::atomic<size_t> last_transaction_id_;
        std::vector<boost::thread> appliers_;
        std::vector<std::shared_ptr<db::client>> clients_;
        std::vector<boost::thread> client_threads_;
    };
};

#endif // WSREP_DB_SERVER_HPP
