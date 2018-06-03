//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_CLIENT_CONTEXT_HPP
#define WSREP_MOCK_CLIENT_CONTEXT_HPP

#include "wsrep/client_context.hpp"
#include "wsrep/mutex.hpp"
#include "wsrep/compiler.hpp"

namespace wsrep
{
    class mock_client_context : public wsrep::client_context
    {
    public:
        mock_client_context(wsrep::server_context& server_context,
                            const wsrep::client_id& id,
                            enum wsrep::client_context::mode mode,
                            bool do_2pc = false)
            : wsrep::client_context(mutex_, server_context, id, mode)
              // Note: Mutex is initialized only after passed
              // to client_context constructor.
            , mutex_()
            , do_2pc_(do_2pc)
            , fail_next_applying_()
        { }
        ~mock_client_context()
        {
            if (transaction().active())
            {
                (void)rollback();
            }
        }
        int apply(const wsrep::data&);
        int commit();
        int rollback();
        bool do_2pc() const { return do_2pc_; }
        void will_replay(wsrep::transaction_context&) WSREP_OVERRIDE { }
        int replay(wsrep::transaction_context& tc) WSREP_OVERRIDE
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            tc.state(lock, wsrep::transaction_context::s_committing);
            tc.state(lock, wsrep::transaction_context::s_ordered_commit);
            tc.state(lock, wsrep::transaction_context::s_committed);
            return 0;
        }
        void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>&) const
            WSREP_OVERRIDE { }
        bool killed() const WSREP_OVERRIDE { return false; }
        void abort() const WSREP_OVERRIDE { }
        void store_globals() WSREP_OVERRIDE { }
        void debug_sync(const char*) WSREP_OVERRIDE { }
        void debug_suicide(const char*) WSREP_OVERRIDE
        {
            ::abort();
        }
        void on_error(enum wsrep::client_error) { }
        // Mock state modifiers
        void fail_next_applying(bool fail_next_applying)
        { fail_next_applying_ = fail_next_applying; }
    private:
        wsrep::default_mutex mutex_;
        bool do_2pc_;
        bool fail_next_applying_;
    };
}

#endif // WSREP_MOCK_CLIENT_CONTEXT_HPP
