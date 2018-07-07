//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_CONTEXT_HPP
#define WSREP_DB_CLIENT_CONTEXT_HPP

#include "wsrep/client_state.hpp"
#include "db_server_state.hpp"

namespace db
{
    class client;
    class client_state : public wsrep::client_state
    {
    public:
        client_state(wsrep::mutex& mutex,
                     wsrep::condition_variable& cond,
                     db::server_state& server_state,
                     wsrep::client_service& client_service,
                     const wsrep::client_id& client_id,
                     enum wsrep::client_state::mode mode)
            : wsrep::client_state(mutex,
                                  cond,
                                  server_state,
                                  client_service,
                                  client_id,
                                  mode)
        { }

    private:
        client_state(const client_state&);
        client_state& operator=(const client_state&);
    };
}

#endif // WSREP_DB_CLIENT_CONTEXT
