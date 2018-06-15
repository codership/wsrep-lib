//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_CONTEXT_HPP
#define WSREP_DB_CLIENT_CONTEXT_HPP

#include "wsrep/client_context.hpp"
#include "db_server_context.hpp"

namespace db
{
    class client_context : public wsrep::client_context
    {
    public:
        client_context(wsrep::mutex& mutex,
                       db::server_context& server_context,
                       wsrep::client_service& client_service,
                       const wsrep::client_id& client_id,
                       enum wsrep::client_context::mode mode)
            : wsrep::client_context(mutex,
                                    server_context,
                                    client_service,
                                    client_id,
                                    mode)
            , is_autocommit_(false)
            , do_2pc_(false)
        { }
        void reset_globals() { }
        void store_globals() { }
        bool is_autocommit() const { return is_autocommit_; }
        bool do_2pc() const { return do_2pc_; }
    private:
        bool is_autocommit_;
        bool do_2pc_;
    };
}

#endif // WSREP_DB_CLIENT_CONTEXT
