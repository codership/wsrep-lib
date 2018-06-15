//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_HPP
#define WSREP_DB_CLIENT_HPP

#include "db_client_context.hpp"
#include "db_server_context.hpp"
#include "db_client_service.hpp"
#include "db_storage_engine.hpp"

namespace db
{
    class server;
    class client
    {
    public:
        struct stats
        {
            long long commits;
            long long rollbacks;
            long long replays;
            stats()
                : commits(0)
                , rollbacks(0)
                , replays(0)
            { }
        };

        client(db::server&,
               wsrep::client_id,
               enum wsrep::client_context::mode,
               const db::params&);

        bool bf_abort(wsrep::seqno);
        const struct stats stats() const { return stats_; }

        void store_globals() { }
        void reset_globals() { }
        void start();

    private:
        friend class db::server_context;
        template <class F> int client_command(F f);
        void run_one_transaction();
        void reset_error();
        wsrep::default_mutex mutex_;
        const db::params& params_;
        db::server_context& server_context_;
        db::client_context client_context_;
        db::client_service client_service_;
        db::storage_engine::transaction se_trx_;
        struct stats stats_;
    };
}

#endif // WSREP_DB_CLIENT_HPP
