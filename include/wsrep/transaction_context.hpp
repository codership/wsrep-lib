//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_TRANSACTION_CONTEXT_HPP
#define WSREP_TRANSACTION_CONTEXT_HPP

#include "provider.hpp"
#include "server_context.hpp"
#include "transaction_id.hpp"
#include "lock.hpp"

#include <cassert>
#include <vector>

namespace wsrep
{
    class client_context;
    class key;
    class const_buffer;


    class transaction_context
    {
    public:
        enum state
        {
            s_executing,
            s_preparing,
            s_certifying,
            s_committing,
            s_ordered_commit,
            s_committed,
            s_cert_failed,
            s_must_abort,
            s_aborting,
            s_aborted,
            s_must_replay,
            s_replaying
        };
        static const int n_states = s_replaying + 1;
        enum state state() const
        { return state_; }

        transaction_context(wsrep::client_context& client_context);
        ~transaction_context();
        // Accessors
        wsrep::transaction_id id() const
        { return id_; }

        bool active() const
        { return (id_ != wsrep::transaction_id::invalid()); }


        void state(wsrep::unique_lock<wsrep::mutex>&, enum state);

        // Return true if the certification of the last
        // fragment succeeded
        bool certified() const { return certified_; }

        wsrep::seqno seqno() const
        {
            return ws_meta_.seqno();
        }
        // Return true if the last fragment was ordered by the
        // provider
        bool ordered() const
        { return (ws_meta_.seqno().nil() == false); }

        /*!
         * Return true if any fragments have been succesfully certified
         * for the transaction.
         */
        bool is_streaming() const
        {
            return (streaming_context_.fragments_certified() > 0);
        }

        bool pa_unsafe() const { return pa_unsafe_; }
        void pa_unsafe(bool pa_unsafe) { pa_unsafe_ = pa_unsafe; }

        //
        int start_transaction()
        {
            assert(active() == false);
            assert(ws_meta_.transaction_id() != transaction_id::invalid());
            return start_transaction(ws_meta_.transaction_id());
        }

        int start_transaction(const wsrep::transaction_id& id);

        int start_transaction(const wsrep::ws_handle& ws_handle,
                              const wsrep::ws_meta& ws_meta);

        int append_key(const wsrep::key&);

        int append_data(const wsrep::const_buffer&);

        int after_row();

        int before_prepare();

        int after_prepare();

        int before_commit();

        int ordered_commit();

        int after_commit();

        int before_rollback();

        int after_rollback();

        int before_statement();

        int after_statement();

        bool bf_abort(wsrep::unique_lock<wsrep::mutex>& lock,
                      wsrep::seqno bf_seqno);

        int flags() const
        {
            return flags_;
        }

        wsrep::mutex& mutex();
        wsrep::ws_handle& ws_handle() { return ws_handle_; }
        const wsrep::ws_handle& ws_handle() const { return ws_handle_; }
        const wsrep::ws_meta& ws_meta() const { return ws_meta_; }
    private:
        transaction_context(const transaction_context&);
        transaction_context operator=(const transaction_context&);

        void flags(int flags) { flags_ = flags; }
        int certify_fragment(wsrep::unique_lock<wsrep::mutex>&);
        int certify_commit(wsrep::unique_lock<wsrep::mutex>&);
        void streaming_rollback();
        void clear_fragments();
        void cleanup();
        void debug_log_state(const char*) const;

        wsrep::provider& provider_;
        wsrep::client_context& client_context_;
        wsrep::transaction_id id_;
        enum state state_;
        std::vector<enum state> state_hist_;
        enum state bf_abort_state_;
        int bf_abort_client_state_;
        wsrep::ws_handle ws_handle_;
        wsrep::ws_meta ws_meta_;
        int flags_;
        bool pa_unsafe_;
        bool certified_;

    public:
        class streaming_context
        {
        public:
            enum fragment_unit
            {
                row,
                bytes,
                statement
            };

            streaming_context()
                : fragments_()
                , rollback_replicated_for_()
                , fragment_unit_()
                , fragment_size_()
                , bytes_certified_()
                , unit_counter_()
            { }

            void enable(enum fragment_unit fragment_unit, size_t fragment_size)
            {
                assert(fragment_size > 0);
                fragment_unit_ = fragment_unit;
                fragment_size_ = fragment_size;
            }

            enum fragment_unit fragment_unit() const { return fragment_unit_; }

            size_t fragment_size() const { return fragment_size_; }

            void disable()
            {
                fragment_size_ = 0;
            }

            void certified(wsrep::seqno seqno)
            {
                fragments_.push_back(seqno);
            }

            size_t fragments_certified() const
            {
                return fragments_.size();
            }

            size_t bytes_certified() const
            {
                return bytes_certified_;
            }

            void rolled_back(wsrep::transaction_id id)
            {
                assert(rollback_replicated_for_ == wsrep::transaction_id::invalid());
                rollback_replicated_for_ = id;
            }

            bool rolled_back() const
            {
                return (rollback_replicated_for_ !=
                        wsrep::transaction_id::invalid());
            }

            size_t unit_counter() const { return unit_counter_; }
            void increment_unit_counter() { ++unit_counter_; }

            void cleanup()
            {
                fragments_.clear();
                rollback_replicated_for_ = wsrep::transaction_id::invalid();
                bytes_certified_ = 0;
                unit_counter_ = 0;
            }
        private:
            std::vector<wsrep::seqno> fragments_;
            wsrep::transaction_id rollback_replicated_for_;
            enum fragment_unit fragment_unit_;
            size_t fragment_size_;
            size_t bytes_certified_;
            size_t unit_counter_;
        } streaming_context_;
    };

    static inline std::string to_string(enum wsrep::transaction_context::state state)
    {
        switch (state)
        {
        case wsrep::transaction_context::s_executing: return "executing";
        case wsrep::transaction_context::s_preparing: return "preparing";
        case wsrep::transaction_context::s_certifying: return "certifying";
        case wsrep::transaction_context::s_committing: return "committing";
        case wsrep::transaction_context::s_ordered_commit: return "ordered_commit";
        case wsrep::transaction_context::s_committed: return "committed";
        case wsrep::transaction_context::s_cert_failed: return "cert_failed";
        case wsrep::transaction_context::s_must_abort: return "must_abort";
        case wsrep::transaction_context::s_aborting: return "aborting";
        case wsrep::transaction_context::s_aborted: return "aborted";
        case wsrep::transaction_context::s_must_replay: return "must_replay";
        case wsrep::transaction_context::s_replaying: return "replaying";
        }
        return "unknown";
    }

}

#endif // WSREP_TRANSACTION_CONTEXT_HPP
