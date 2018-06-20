//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/** @file transaction.hpp */
#ifndef WSREP_TRANSACTION_CONTEXT_HPP
#define WSREP_TRANSACTION_CONTEXT_HPP

#include "provider.hpp"
#include "server_state.hpp"
#include "transaction_id.hpp"
#include "streaming_context.hpp"
#include "lock.hpp"

#include <cassert>
#include <vector>

namespace wsrep
{
    class client_service;
    class client_state;
    class key;
    class const_buffer;


    class transaction
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

        transaction(wsrep::client_state& client_state);
        ~transaction();
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

        int start_transaction(const wsrep::transaction_id& id);

        int start_transaction(const wsrep::ws_handle& ws_handle,
                              const wsrep::ws_meta& ws_meta);

        int start_replaying(const wsrep::ws_meta&);

        int append_key(const wsrep::key&);

        int append_data(const wsrep::const_buffer&);

        int after_row();

        int before_prepare(wsrep::unique_lock<wsrep::mutex>&);

        int after_prepare(wsrep::unique_lock<wsrep::mutex>&);

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

        // wsrep::mutex& mutex();
        wsrep::ws_handle& ws_handle() { return ws_handle_; }
        const wsrep::ws_handle& ws_handle() const { return ws_handle_; }
        const wsrep::ws_meta& ws_meta() const { return ws_meta_; }
    private:
        transaction(const transaction&);
        transaction operator=(const transaction&);

        wsrep::provider& provider();
        void flags(int flags) { flags_ = flags; }
        int certify_fragment(wsrep::unique_lock<wsrep::mutex>&);
        int certify_commit(wsrep::unique_lock<wsrep::mutex>&);
        void streaming_rollback();
        void clear_fragments();
        void cleanup();
        void debug_log_state(const char*) const;

        wsrep::server_service& server_service_;
        wsrep::client_service& client_service_;
        wsrep::client_state& client_state_;
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
        wsrep::streaming_context streaming_context_;
    };

    static inline const char* to_c_string(enum wsrep::transaction::state state)
    {
        switch (state)
        {
        case wsrep::transaction::s_executing: return "executing";
        case wsrep::transaction::s_preparing: return "preparing";
        case wsrep::transaction::s_certifying: return "certifying";
        case wsrep::transaction::s_committing: return "committing";
        case wsrep::transaction::s_ordered_commit: return "ordered_commit";
        case wsrep::transaction::s_committed: return "committed";
        case wsrep::transaction::s_cert_failed: return "cert_failed";
        case wsrep::transaction::s_must_abort: return "must_abort";
        case wsrep::transaction::s_aborting: return "aborting";
        case wsrep::transaction::s_aborted: return "aborted";
        case wsrep::transaction::s_must_replay: return "must_replay";
        case wsrep::transaction::s_replaying: return "replaying";
        }
        return "unknown";
    }
    static inline std::string to_string(enum wsrep::transaction::state state)
    {
        return to_c_string(state);
    }

}

#endif // WSREP_TRANSACTION_CONTEXT_HPP
