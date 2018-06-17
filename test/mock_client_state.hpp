//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_CLIENT_CONTEXT_HPP
#define WSREP_MOCK_CLIENT_CONTEXT_HPP

#include "wsrep/client_state.hpp"
#include "wsrep/mutex.hpp"
#include "wsrep/compiler.hpp"

#include "test_utils.hpp"

namespace wsrep
{
    class mock_client_state : public wsrep::client_state
    {
    public:
        mock_client_state(wsrep::server_state& server_state,
                            wsrep::client_service& client_service,
                            const wsrep::client_id& id,
                            enum wsrep::client_state::mode mode)
            : wsrep::client_state(mutex_, server_state, client_service, id, mode)
              // Note: Mutex is initialized only after passed
              // to client_state constructor.
            , mutex_()
        { }
        ~mock_client_state()
        {
            if (transaction().active())
            {
                (void)rollback();
            }
        }
    private:
        wsrep::default_mutex mutex_;
    public:
    private:
    };


    class mock_client_service : public wsrep::client_service
    {
    public:
        mock_client_service(wsrep::provider& provider)
            : wsrep::client_service(provider)
            , is_autocommit_()
            , do_2pc_()
            , fail_next_applying_()
            , bf_abort_during_wait_()
            , error_during_prepare_data_()
            , killed_before_certify_()
            , sync_point_enabled_()
            , sync_point_action_()
            , bytes_generated_()
            , replays_()
            , aborts_()
        { }

        int apply(wsrep::client_state&, const wsrep::const_buffer&) WSREP_OVERRIDE;

        int commit(wsrep::client_state&, const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;

        int rollback(wsrep::client_state&) WSREP_OVERRIDE;

        bool is_autocommit() const WSREP_OVERRIDE
        { return is_autocommit_; }

        bool do_2pc() const WSREP_OVERRIDE
        { return do_2pc_; }

        bool interrupted() const WSREP_OVERRIDE
        { return killed_before_certify_; }

        void reset_globals() WSREP_OVERRIDE { }
        void emergency_shutdown() WSREP_OVERRIDE { ++aborts_; }

        int append_fragment(const wsrep::transaction&,
                            int, const wsrep::const_buffer&) WSREP_OVERRIDE
        { return 0; }
        void remove_fragments(const wsrep::transaction& )
            WSREP_OVERRIDE { }
        void will_replay(const wsrep::transaction&)
            WSREP_OVERRIDE { }

        enum wsrep::provider::status
        replay(wsrep::client_state& client_state,
               wsrep::transaction& tc) WSREP_OVERRIDE
        {
            enum wsrep::provider::status ret(
                provider_.replay(tc.ws_handle(), &client_state));
            ++replays_;
            return ret;
        }

        void wait_for_replayers(
            wsrep::client_state& client_state,
            wsrep::unique_lock<wsrep::mutex>& lock)
            WSREP_OVERRIDE
        {
            lock.unlock();
            if (bf_abort_during_wait_)
            {
                wsrep_test::bf_abort_unordered(client_state);
            }
            lock.lock();
        }

        int prepare_data_for_replication(
            wsrep::client_state& client_state,
            const wsrep::transaction&) WSREP_OVERRIDE
        {
            if (error_during_prepare_data_)
            {
                return 1;
            }
            static const char buf[1] = { 1 };
            wsrep::const_buffer data = wsrep::const_buffer(buf, 1);
            return client_state.append_data(data);
        }

        size_t bytes_generated() const
        {
            return bytes_generated_;
        }

        int prepare_fragment_for_replication(
            wsrep::client_state& client_state,
            const wsrep::transaction&,
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
            return client_state.append_data(data);
        }

        void store_globals() WSREP_OVERRIDE { }

        void debug_sync(wsrep::client_state& client_state,
                        const char* sync_point) WSREP_OVERRIDE
        {
            if (sync_point_enabled_ == sync_point)
            {
                switch (sync_point_action_)
                {
                case spa_bf_abort_unordered:
                    wsrep_test::bf_abort_unordered(client_state);
                    break;
                case spa_bf_abort_ordered:
                    wsrep_test::bf_abort_ordered(client_state);
                    break;
                }
            }
        }

        void debug_crash(const char*) WSREP_OVERRIDE
        {
            // Not going to do this while unit testing
        }


        //
        // Knobs to tune the behavior
        //
        bool is_autocommit_;
        bool do_2pc_;
        bool fail_next_applying_;
        bool bf_abort_during_wait_;
        bool error_during_prepare_data_;
        bool killed_before_certify_;
        std::string sync_point_enabled_;
        enum sync_point_action
        {
            spa_bf_abort_unordered,
            spa_bf_abort_ordered
        } sync_point_action_;
        size_t bytes_generated_;

        //
        // Verifying the state
        //
        size_t replays() const { return replays_; }
        size_t aborts() const { return aborts_; }

    private:
        size_t replays_;
        size_t aborts_;
    };

}

#endif // WSREP_MOCK_CLIENT_CONTEXT_HPP
