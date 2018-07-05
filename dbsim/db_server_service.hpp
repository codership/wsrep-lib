//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_SERVICE_HPP
#define WSREP_DB_SERVER_SERVICE_HPP

#include "wsrep/server_service.hpp"
#include <string>

namespace db
{
    class server;
    class server_service : public wsrep::server_service
    {
    public:
        server_service(db::server& server);
        wsrep::client_state* local_client_state() override;
        void release_client_state(wsrep::client_state*) override;
        wsrep::high_priority_service* streaming_applier_service() override;
        void release_high_priority_service(wsrep::high_priority_service*) override;

        bool sst_before_init() const override;
        int start_sst(const std::string&, const wsrep::gtid&, bool) override;
        std::string sst_request() override;
        void background_rollback(wsrep::client_state&) override;
        void bootstrap() override;
        void log_message(enum wsrep::log::level, const char* message);
        void log_dummy_write_set(wsrep::client_state&, const wsrep::ws_meta&)
            override;
        void log_view(const wsrep::view&) override;
        void log_state_change(enum wsrep::server_state::state,
                              enum wsrep::server_state::state) override;
        int wait_committing_transactions(int) override;
        void debug_sync(const char*) override;
    private:
        db::server& server_;
    };
}

#endif // WSREP_DB_SERVER_SERVICE_HPP
