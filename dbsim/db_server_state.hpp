//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_CONTEXT_HPP
#define WSREP_DB_SERVER_CONTEXT_HPP

#include "wsrep/server_state.hpp"
#include "wsrep/client_state.hpp"

#include <atomic>

namespace db
{
    class server;
    class server_state : public wsrep::server_state
    {
    public:
        server_state(db::server& server,
                       const std::string& name,
                       const std::string& server_id,
                       const std::string& address,
                       const std::string& working_dir)
            : wsrep::server_state(
                mutex_,
                cond_,
                name,
                server_id,
                address,
                working_dir,
                wsrep::server_state::rm_async)
            , mutex_()
            , cond_()
            , server_(server)
        { }
        wsrep::client_state* local_client_state() override;
        wsrep::client_state* streaming_applier_client_state() override;
        void release_client_state(wsrep::client_state*) override;
        bool sst_before_init() const override;
        int start_sst(const std::string&, const wsrep::gtid&, bool) override;
        std::string sst_request() override;
        void background_rollback(wsrep::client_state&) override;
        void log_dummy_write_set(wsrep::client_state&, const wsrep::ws_meta&)
            override;
        void log_view(wsrep::client_state&, const wsrep::view&) override;
    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        db::server& server_;
    };
}

#endif // WSREP_DB_SERVER_CONTEXT_HPP
