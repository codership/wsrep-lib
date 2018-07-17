//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_CLIENT_SERVICE_HPP
#define WSREP_DB_CLIENT_SERVICE_HPP

#include "wsrep/client_service.hpp"
#include "wsrep/transaction.hpp"

namespace db
{
    class client;
    class client_state;

    class client_service : public wsrep::client_service
    {
    public:
        client_service(db::client& client);

        bool do_2pc() const override;
        bool interrupted() const override { return false; }
        void reset_globals() override { }
        void store_globals() override { }
        int prepare_data_for_replication() override
        {
            return 0;
        }
        void cleanup_transaction() override { }
        size_t bytes_generated() const override
        {
            return 0;
        }
        bool statement_allowed_for_streaming() const override
        {
            return true;
        }
        int prepare_fragment_for_replication(wsrep::mutable_buffer&) override
        {
            return 0;
        }
        int remove_fragments() override { return 0; }
        int bf_rollback() override;
        void will_replay() override { }
        void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>&) override { }
        enum wsrep::provider::status replay()
            override;

        void emergency_shutdown() { ::abort(); }
        void debug_sync(const char*) override { }
        void debug_crash(const char*) override { }
    private:
        db::client& client_;
        wsrep::client_state& client_state_;
    };
}

#endif // WSREP_DB_CLIENT_SERVICE_HPP
