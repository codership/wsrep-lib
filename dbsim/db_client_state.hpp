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
                     db::client* client,
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
            , client_(client)
            , is_autocommit_(false)
            , do_2pc_(false)
        { }
        db::client* client() { return client_; }
        void reset_globals() { }
        void store_globals() { wsrep::client_state::store_globals(); }
        bool is_autocommit() const { return is_autocommit_; }
        bool do_2pc() const { return do_2pc_; }

    private:
        client_state(const client_state&);
        client_state& operator=(const client_state&);
        db::client* client_;
        bool is_autocommit_;
        bool do_2pc_;
    };
}

#endif // WSREP_DB_CLIENT_CONTEXT
