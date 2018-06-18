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
        db::server& server_;
    };
}

#endif // WSREP_DB_SERVER_SERVICE_HPP
