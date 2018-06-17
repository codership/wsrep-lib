//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_HPP
#define WSREP_DB_CLIENT_HPP

#include "db_server_state.hpp"
#include "db_storage_engine.hpp"
#include "db_client_state.hpp"
#include "db_client_service.hpp"


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
               enum wsrep::client_state::mode,
               const db::params&);
        bool bf_abort(wsrep::seqno);
        const struct stats stats() const { return stats_; }
        void store_globals()
        {
            client_state_.store_globals();
        }
        void reset_globals()
        { }
        void start();
        wsrep::client_state& client_state() { return client_state_; }
    private:
        friend class db::server_state;
        friend class db::client_service;
        template <class F> int client_command(F f);
        void run_one_transaction();
        void reset_error();
        void report_progress(size_t) const;
        wsrep::default_mutex mutex_;
        const db::params& params_;
        db::server& server_;
        db::server_state& server_state_;
        db::client_state client_state_;
        db::client_service client_service_;
        db::storage_engine::transaction se_trx_;
        struct stats stats_;
    };
}

#endif // WSREP_DB_CLIENT_HPP
