//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_CONTEXT_HPP
#define WSREP_DB_SERVER_CONTEXT_HPP

#include "wsrep/server_context.hpp"
#include "wsrep/client_context.hpp"

#include <atomic>

namespace db
{
    class server_context : public wsrep::server_context
    {
    public:
        server_context(const std::string& name,
                       const std::string& server_id,
                       const std::string& address,
                       const std::string& working_dir)
            : wsrep::server_context(
                mutex_,
                cond_,
                name,
                server_id,
                address,
                working_dir,
                wsrep::server_context::rm_async)
            , mutex_()
            , cond_()
            , last_transaction_id_()
        { }
        wsrep::client_context* local_client_context() override;
        wsrep::client_context* streaming_applier_client_context() override;
        void release_client_context(wsrep::client_context*) override;
        bool sst_before_init() const override;
        void on_sst_request(const std::string&, const wsrep::gtid&, bool) override;
        std::string on_sst_required() override;
        void background_rollback(wsrep::client_context&) override;
        void log_dummy_write_set(wsrep::client_context&, const wsrep::ws_meta&)
            override;
        size_t next_transaction_id()
        {
            return (last_transaction_id_.fetch_add(1) + 1);
        }
    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        std::atomic<size_t> last_transaction_id_;
    };
}

#endif // WSREP_DB_SERVER_CONTEXT_HPP
