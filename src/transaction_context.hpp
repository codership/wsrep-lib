//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_TRANSACTION_CONTEXT_HPP
#define TRREP_TRANSACTION_CONTEXT_HPP

#include "provider.hpp"
#include "server_context.hpp"
#include "lock.hpp"

#include <wsrep_api.h>

#include <cassert>
#include <vector>

namespace trrep
{
    class client_context;
    class key;
    class data;

    class transaction_id
    {
    public:
        template <typename I>
        transaction_id(I id)
            : id_(static_cast<wsrep_trx_id_t>(id))
        { }
        wsrep_trx_id_t get() const { return id_; }
        static wsrep_trx_id_t invalid() { return wsrep_trx_id_t(-1); }
        bool operator==(const transaction_id& other) const
        { return (id_ == other.id_); }
        bool operator!=(const transaction_id& other) const
        { return (id_ != other.id_); }
    private:
        wsrep_trx_id_t id_;
    };

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

        transaction_context(trrep::client_context& client_context);
        ~transaction_context();
        // Accessors
        trrep::transaction_id id() const
        { return id_; }

        bool active() const
        { return (id_ != trrep::transaction_id::invalid()); }


        void state(trrep::unique_lock<trrep::mutex>&, enum state);

        // Return true if the certification of the last
        // fragment succeeded
        bool certified() { return certified_; }

        // Return true if the last fragment was ordered by the
        // provider
        bool ordered() { return (trx_meta_.gtid.seqno > 0); }

        bool is_streaming() const
        {
            // Streaming support not yet implemented
            return false;
        }

        bool pa_unsafe() const { return pa_unsafe_; }
        void pa_unsafe(bool pa_unsafe) { pa_unsafe_ = pa_unsafe; }
        //
        int start_transaction()
        {
            assert(active() == false);
            assert(trx_meta_.stid.trx != transaction_id::invalid());
            return start_transaction(trx_meta_.stid.trx);
        }

        int start_transaction(const trrep::transaction_id& id);

        int start_transaction(const wsrep_ws_handle_t& ws_handle,
                              const wsrep_trx_meta_t& trx_meta,
                              uint32_t flags);

        int append_key(const trrep::key&);

        int append_data(const trrep::data&);

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

        uint32_t flags() const
        {
            return flags_;
        }
    private:
        transaction_context(const transaction_context&);
        transaction_context operator=(const transaction_context&);

        int certify_fragment(trrep::unique_lock<trrep::mutex>&);
        int certify_commit(trrep::unique_lock<trrep::mutex>&);
        void remove_fragments();
        void clear_fragments();
        void cleanup();
        void debug_log_state(const std::string&) const;
        trrep::provider& provider_;
        trrep::client_context& client_context_;
        trrep::transaction_id id_;
        enum state state_;
        std::vector<enum state> state_hist_;
        wsrep_ws_handle_t ws_handle_;
        wsrep_trx_meta_t trx_meta_;
        uint32_t flags_;
        bool pa_unsafe_;
        bool certified_;

        std::vector<wsrep_gtid_t> fragments_;
        trrep::transaction_id rollback_replicated_for_;
    };

    static inline std::string to_string(enum trrep::transaction_context::state state)
    {
        switch (state)
        {
        case trrep::transaction_context::s_executing: return "executing";
        case trrep::transaction_context::s_preparing: return "preparing";
        case trrep::transaction_context::s_certifying: return "certifying";
        case trrep::transaction_context::s_committing: return "committing";
        case trrep::transaction_context::s_ordered_commit: return "ordered_commit";
        case trrep::transaction_context::s_committed: return "committed";
        case trrep::transaction_context::s_cert_failed: return "cert_failed";
        case trrep::transaction_context::s_must_abort: return "must_abort";
        case trrep::transaction_context::s_aborting: return "aborting";
        case trrep::transaction_context::s_aborted: return "aborted";
        case trrep::transaction_context::s_must_replay: return "must_replay";
        case trrep::transaction_context::s_replaying: return "replaying";
        }
        return "unknown";
    }

}

#endif // TRREP_TRANSACTION_CONTEXT_HPP
