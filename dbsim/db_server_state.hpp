//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_CONTEXT_HPP
#define WSREP_DB_SERVER_CONTEXT_HPP

#include "wsrep/server_state.hpp"
#include "wsrep/server_service.hpp"
#include "wsrep/client_state.hpp"

#include <atomic>

namespace db
{
    class server;
    class server_state : public wsrep::server_state
    {
    public:
        server_state(db::server& server,
                     wsrep::server_service& server_service,
                     const std::string& name,
                     const std::string& server_id,
                     const std::string& address,
                     const std::string& working_dir)
            : wsrep::server_state(
                mutex_,
                cond_,
                server_service,
                name,
                server_id,
                "",
                address,
                working_dir,
                wsrep::gtid::undefined(),
                1,
                wsrep::server_state::rm_async)
            , mutex_()
            , cond_()
            , server_(server)
        { }
    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        db::server& server_;
    };
}

#endif // WSREP_DB_SERVER_CONTEXT_HPP
