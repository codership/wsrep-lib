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
            , sync_point_action_()
            , replays_()
            , aborts_()
        { }
        ~mock_client_context()
        {
            if (transaction().active())
            {
                (void)rollback();
            }
        }
        int apply(const wsrep::const_buffer&);
        int commit();
        int rollback();
        bool is_autocommit() const { return is_autocommit_; }
        bool do_2pc() const { return do_2pc_; }
        int append_fragment(const wsrep::transaction_context&,
                            int, const wsrep::const_buffer&) WSREP_OVERRIDE
        { return 0; }
        void remove_fragments(const wsrep::transaction_context& )
            WSREP_OVERRIDE { }
        void will_replay(wsrep::transaction_context&) WSREP_OVERRIDE { }
        int replay(wsrep::transaction_context& tc) WSREP_OVERRIDE
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            tc.state(lock, wsrep::transaction_context::s_committing);
            tc.state(lock, wsrep::transaction_context::s_ordered_commit);
            tc.state(lock, wsrep::transaction_context::s_committed);
            ++replays_;
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
            const wsrep::transaction_context&) WSREP_OVERRIDE
        {
            if (error_during_prepare_data_)
            {
                return 1;
            }
            static const char buf[1] = { 1 };
            wsrep::const_buffer data = wsrep::const_buffer(buf, 1);
            return transaction_.append_data(data);
        }

        int prepare_fragment_for_replication(const wsrep::transaction_context&,
                                             wsrep::mutable_buffer& buffer)
            WSREP_OVERRIDE
        {
            if (error_during_prepare_data_)
            {
                return 1;
            }
            static const char buf[1] = { 1 };
            buffer.push_back(&buf[0], &buf[1]);
            wsrep::const_buffer data(buffer.data(), buffer.size());
            return transaction_.append_data(data);
        }

        bool killed() const WSREP_OVERRIDE { return killed_before_certify_; }
        void abort() WSREP_OVERRIDE { ++aborts_; }
        void store_globals() WSREP_OVERRIDE { }
        void debug_sync(wsrep::unique_lock<wsrep::mutex>& lock,
                        const char* sync_point) WSREP_OVERRIDE
        {
            lock.unlock();
            if (sync_point_action_ == sync_point)
            {
                wsrep_test::bf_abort_ordered(*this);
            }
            lock.lock();
        }
        void debug_suicide(const char*) WSREP_OVERRIDE
        {
            // Not going to do this while unit testing
        }
        void on_error(enum wsrep::client_error) { }

        size_t replays() const { return replays_; }
        size_t aborts() const { return aborts_; }
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
        std::string sync_point_action_;
    private:
        size_t replays_;
        size_t aborts_;
    };
}

#endif // WSREP_MOCK_CLIENT_CONTEXT_HPP
