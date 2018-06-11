//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_CLIENT_CONTEXT_HPP
#define WSREP_MOCK_CLIENT_CONTEXT_HPP

#include "wsrep/client_context.hpp"
#include "wsrep/mutex.hpp"
#include "wsrep/compiler.hpp"

#include "test_utils.hpp"

namespace wsrep
{
    class mock_client_context : public wsrep::client_context
    {
    public:
        mock_client_context(wsrep::server_context& server_context,
                            const wsrep::client_id& id,
                            enum wsrep::client_context::mode mode,
                            bool is_autocommit = false,
                            bool do_2pc = false)
            : wsrep::client_context(mutex_, server_context, id, mode)
              // Note: Mutex is initialized only after passed
              // to client_context constructor.
            , mutex_()
            , is_autocommit_(is_autocommit)
            , do_2pc_(do_2pc)
            , fail_next_applying_()
            , bf_abort_during_wait_()
            , error_during_prepare_data_()
            , killed_before_certify_()
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
        bool is_autocommit() const { return is_autocommit_; }
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
        void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>& lock)
            WSREP_OVERRIDE
        {
            lock.unlock();
            if (bf_abort_during_wait_)
            {
                wsrep_test::bf_abort_unordered(*this);
            }
            lock.lock();
        }
        int prepare_data_for_replication(
            const wsrep::transaction_context&, wsrep::data& data) WSREP_OVERRIDE
        {
            if (error_during_prepare_data_)
            {
                return 1;
            }
            static const char buf[1] = { 1 };
            data = wsrep::data(buf, 1);
            return 0;

        }
        bool killed() const WSREP_OVERRIDE { return killed_before_certify_; }
        void abort() const WSREP_OVERRIDE { }
        void store_globals() WSREP_OVERRIDE { }
        void debug_sync(const char*) WSREP_OVERRIDE { }
        void debug_suicide(const char*) WSREP_OVERRIDE
        {
            ::abort();
        }
        void on_error(enum wsrep::client_error) { }

        //
    private:
        wsrep::default_mutex mutex_;
    public:
        bool is_autocommit_;
        bool do_2pc_;
        bool fail_next_applying_;
        bool bf_abort_during_wait_;
        bool error_during_prepare_data_;
        bool killed_before_certify_;
    };
}

#endif // WSREP_MOCK_CLIENT_CONTEXT_HPP
