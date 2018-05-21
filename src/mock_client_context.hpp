//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MOCK_CLIENT_CONTEXT_HPP
#define TRREP_MOCK_CLIENT_CONTEXT_HPP

#include "trrep/client_context.hpp"
#include "trrep/mutex.hpp"
#include "trrep/compiler.hpp"

namespace trrep
{
    class mock_client_context : public trrep::client_context
    {
    public:
        mock_client_context(trrep::server_context& server_context,
                            const trrep::client_id& id,
                            enum trrep::client_context::mode mode,
                            bool do_2pc = false)
            : trrep::client_context(mutex_, server_context, id, mode)
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
                (void)rollback(transaction());
            }
        }
        int apply(trrep::transaction_context&, const trrep::data&);
        int commit(trrep::transaction_context&);
        int rollback(trrep::transaction_context&);
        bool do_2pc() const { return do_2pc_; }
        void will_replay(trrep::transaction_context&) TRREP_OVERRIDE { }
        int replay(trrep::transaction_context& tc) TRREP_OVERRIDE
        {
            trrep::unique_lock<trrep::mutex> lock(mutex_);
            tc.state(lock, trrep::transaction_context::s_committing);
            tc.state(lock, trrep::transaction_context::s_ordered_commit);
            tc.state(lock, trrep::transaction_context::s_committed);
            return 0;
        }
        void wait_for_replayers(trrep::unique_lock<trrep::mutex>&) const
            TRREP_OVERRIDE { }
        bool killed() const TRREP_OVERRIDE { return false; }
        void abort() const TRREP_OVERRIDE { }
        void store_globals() TRREP_OVERRIDE { }
        void debug_sync(const char*) TRREP_OVERRIDE { }
        void debug_suicide(const char*) TRREP_OVERRIDE
        {
            ::abort();
        }
        void on_error(enum trrep::client_error) { }
        // Mock state modifiers
        void fail_next_applying(bool fail_next_applying)
        { fail_next_applying_ = fail_next_applying; }
    private:
        trrep::default_mutex mutex_;
        bool do_2pc_;
        bool fail_next_applying_;
    };
}

#endif // TRREP_MOCK_CLIENT_CONTEXT_HPP
