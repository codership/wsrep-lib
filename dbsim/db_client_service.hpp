//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_SERVICE_HPP
#define WSREP_DB_CLIENT_SERVICE_HPP

#include "wsrep/client_service.hpp"
#include "wsrep/transaction.hpp"

#include "db_client_state.hpp"

namespace db
{
    class client_service : public wsrep::client_service
    {
    public:
        client_service(wsrep::provider& provider,
                       db::client_state& client_state)
            : wsrep::client_service(provider)
            , client_state_(client_state)
        { }

        bool is_autocommit() const override
        {
            return client_state_.is_autocommit();
        }

        bool do_2pc() const override
        {
            return client_state_.do_2pc();
        }

        bool interrupted() const override
        {
            return false;
        }

        void reset_globals() override
        {
            client_state_.reset_globals();
        }

        void store_globals() override
        {
            client_state_.store_globals();
        }

        int prepare_data_for_replication(wsrep::client_state&, const wsrep::transaction&) override
        {
            return 0;
        }

        size_t bytes_generated() const override
        {
            return 0;
        }

        int prepare_fragment_for_replication(wsrep::client_state&,
                                             const wsrep::transaction&,
                                             wsrep::mutable_buffer&) override
        {
            return 0;
        }

        void remove_fragments(const wsrep::transaction&) override
        { }

        int apply(wsrep::client_state&, const wsrep::const_buffer&) override;

        int commit(wsrep::client_state&,
                   const wsrep::ws_handle&, const wsrep::ws_meta&) override;

        int rollback(wsrep::client_state&) override;

        void will_replay(const wsrep::transaction&) override
        { }

        void wait_for_replayers(wsrep::client_state&,
                                wsrep::unique_lock<wsrep::mutex>&) override { }
        enum wsrep::provider::status replay(wsrep::client_state&,
                                            wsrep::transaction&)
            override;

        int append_fragment(const wsrep::transaction&, int,
                            const wsrep::const_buffer&) override
        {
            return 0;
        }

        void emergency_shutdown() { ::abort(); }
        void debug_sync(wsrep::client_state&, const char*) override { }
        void debug_crash(const char*) override { }
    private:
        db::client_state& client_state_;
    };
}

#endif // WSREP_DB_CLIENT_SERVICE_HPP
