//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_SREVICE_HPP
#define WSREP_DB_CLIENT_SERVICE_HPP

#include "wsrep/client_service.hpp"
#include "wsrep/client_context.hpp"
#include "wsrep/transaction_context.hpp"

#include "db_client_context.hpp"

namespace db
{
    class client_service : public wsrep::client_service
    {
    public:
        client_service(wsrep::provider& provider,
                       db::client_context& client_context)
            : wsrep::client_service(provider)
            , client_context_(client_context)
        { }

        bool is_autocommit() const override
        {
            return client_context_.is_autocommit();
        }

        bool do_2pc() const override
        {
            return client_context_.do_2pc();
        }

        bool interrupted() const override
        {
            return false;
        }

        void reset_globals() override
        {
            client_context_.reset_globals();
        }

        void store_globals() override
        {
            client_context_.store_globals();
        }

        int prepare_data_for_replication(wsrep::client_context&, const wsrep::transaction_context&) override
        {
            return 0;
        }

        size_t bytes_generated() const override
        {
            return 0;
        }

        int prepare_fragment_for_replication(wsrep::client_context&,
                                             const wsrep::transaction_context&,
                                             wsrep::mutable_buffer&) override
        {
            return 0;
        }

        void remove_fragments(const wsrep::transaction_context&) override
        { }

        int apply(wsrep::client_context&, const wsrep::const_buffer&) override
        {
            return 0;
        }

        int commit(wsrep::client_context&,
                   const wsrep::ws_handle&, const wsrep::ws_meta&) override
        {
            return 0;
        }

        int rollback(wsrep::client_context&) override
        {
            return 0;
        }

        void will_replay(const wsrep::transaction_context&) override
        { }

        void wait_for_replayers(wsrep::client_context&,
                                wsrep::unique_lock<wsrep::mutex>&) override { }
        enum wsrep::provider::status replay(wsrep::client_context&,
                                            wsrep::transaction_context&)
            override
        { return wsrep::provider::success; }

        int append_fragment(const wsrep::transaction_context&, int,
                            const wsrep::const_buffer&) override
        {
            return 0;
        }

        void emergency_shutdown() { ::abort(); }
        void debug_sync(wsrep::client_context&, const char*) override { }
        void debug_crash(const char*) override { }
    private:
        db::client_context& client_context_;
    };
}

#endif // WSREP_DB_CLIENT_SERVICE_HPP
