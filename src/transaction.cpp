//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction.hpp"
#include "wsrep/client_state.hpp"
#include "wsrep/server_state.hpp"
#include "wsrep/key.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"

#include <sstream>
#include <memory>

#define WSREP_TC_LOG_DEBUG(level, msg)                  \
    do {                                                \
        if (client_state_.debug_log_level() < level)  \
        { }                                             \
        else wsrep::log_debug() << msg;                 \
    } while (0)

// Public

wsrep::transaction::transaction(
    wsrep::client_state& client_state)
    : server_service_(client_state.server_state().server_service())
    , client_service_(client_state.client_service())
    , client_state_(client_state)
    , id_(transaction_id::undefined())
    , state_(s_executing)
    , state_hist_()
    , bf_abort_state_(s_executing)
    , bf_abort_client_state_()
    , ws_handle_()
    , ws_meta_()
    , flags_()
    , pa_unsafe_(false)
    , certified_(false)
    , streaming_context_()
{ }


wsrep::transaction::~transaction()
{
}

int wsrep::transaction::start_transaction(
    const wsrep::transaction_id& id)
{
    assert(active() == false);
    id_ = id;
    state_ = s_executing;
    state_hist_.clear();
    ws_handle_ = wsrep::ws_handle(id);
    flags_ |= wsrep::provider::flag::start_transaction;
    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
    case wsrep::client_state::m_high_priority:
        return 0;
    case wsrep::client_state::m_replicating:
        return provider().start_transaction(ws_handle_);
    default:
        assert(0);
        return 1;
    }
}

int wsrep::transaction::start_transaction(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    assert(ws_meta.flags());
    assert(active() == false);
    id_ = ws_meta.transaction_id();
    assert(client_state_.mode() == wsrep::client_state::m_high_priority);
    state_ = s_executing;
    state_hist_.clear();
    ws_handle_ = ws_handle;
    ws_meta_ = ws_meta;
    certified_ = true;
    return 0;
}

int wsrep::transaction::start_replaying(const wsrep::ws_meta& ws_meta)
{
    ws_meta_ = ws_meta;
    assert(ws_meta_.flags() & wsrep::provider::flag::commit);
    assert(active());
    assert(client_state_.mode() == wsrep::client_state::m_high_priority);
    assert(state() == s_replaying);
    assert(ws_meta_.seqno().is_undefined() == false);
    certified_ = true;
    return 0;
}

int wsrep::transaction::append_key(const wsrep::key& key)
{
    /** @todo Collect table level keys for SR commit */
    return provider().append_key(ws_handle_, key);
}

int wsrep::transaction::append_data(const wsrep::const_buffer& data)
{

    return provider().append_data(ws_handle_, data);
}

int wsrep::transaction::after_row()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    if (streaming_context_.fragment_size() > 0)
    {
        switch (streaming_context_.fragment_unit())
        {
        case streaming_context::row:
            streaming_context_.increment_unit_counter(1);
            if (streaming_context_.unit_counter() >=
                streaming_context_.fragment_size())
            {
                streaming_context_.reset_unit_counter();
                return certify_fragment(lock);
            }
            break;
        case streaming_context::bytes:
            if (client_service_.bytes_generated() >=
                streaming_context_.bytes_certified()
                + streaming_context_.fragment_size())
            {
                return certify_fragment(lock);
            }
            break;
        case streaming_context::statement:
            // This case is checked in after_statement()
            break;
        }
    }
    return 0;
}

int wsrep::transaction::before_prepare(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    int ret(0);
    debug_log_state("before_prepare_enter");
    assert(state() == s_executing || state() == s_must_abort ||
           state() == s_replaying);

    if (state() == s_must_abort)
    {
        assert(client_state_.mode() == wsrep::client_state::m_replicating);
        client_state_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_preparing);

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_replicating:
        if (is_streaming())
        {
            client_service_.debug_crash(
                "crash_last_fragment_commit_before_fragment_removal");
            lock.unlock();
            if (client_service_.statement_allowed_for_streaming() == false)
            {
                client_state_.override_error(wsrep::e_error_during_commit);
                ret = 1;
            }
            else
            {
                client_service_.remove_fragments();
            }
            lock.lock();
            client_service_.debug_crash(
                "crash_last_fragment_commit_after_fragment_removal");
        }
        break;
    case wsrep::client_state::m_local:
    case wsrep::client_state::m_high_priority:
        if (is_streaming())
        {
            client_service_.remove_fragments();
        }
        break;
    default:
        assert(0);
        break;
    }

    assert(state() == s_preparing);
    debug_log_state("before_prepare_leave");
    return ret;
}

int wsrep::transaction::after_prepare(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    int ret(1);
    debug_log_state("after_prepare_enter");
    assert(state() == s_preparing || state() == s_must_abort);
    if (state() == s_must_abort)
    {
        assert(client_state_.mode() == wsrep::client_state::m_replicating);
        client_state_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_replicating:
        ret = certify_commit(lock);
        assert((ret == 0 && state() == s_committing) ||
               (state() == s_must_abort ||
                state() == s_must_replay ||
                state() == s_cert_failed));
        break;
    case wsrep::client_state::m_local:
    case wsrep::client_state::m_high_priority:
        state(lock, s_certifying);
        state(lock, s_committing);
        ret = 0;
        break;
    default:
        assert(0);
        break;
    }
    debug_log_state("after_prepare_leave");
    return ret;
}

int wsrep::transaction::before_commit()
{
    int ret(1);

    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("before_commit_enter");
    assert(client_state_.mode() != wsrep::client_state::m_toi);
    assert(state() == s_executing ||
           state() == s_committing ||
           state() == s_must_abort ||
           state() == s_replaying);
    assert((state() != s_committing && state() != s_replaying) ||
           certified());

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
        if (ordered())
        {
            ret = provider().commit_order_enter(ws_handle_, ws_meta_);
        }
        break;
    case wsrep::client_state::m_replicating:
        if (state() == s_executing)
        {
            assert(client_service_.do_2pc() == false);
            ret = before_prepare(lock) || after_prepare(lock);
            assert((ret == 0 && state() == s_committing)
                   ||
                   (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed));
        }
        else if (state() != s_committing)
        {
            assert(state() == s_must_abort);
            if (certified())
            {
                state(lock, s_must_replay);
            }
            else
            {
                client_state_.override_error(wsrep::e_deadlock_error);
            }
        }
        else
        {
            // 2PC commit, prepare was done before
            ret = 0;
        }
        if (ret == 0)
        {
            assert(certified());
            assert(ordered());
            lock.unlock();
            enum wsrep::provider::status
                status(provider().commit_order_enter(ws_handle_, ws_meta_));
            lock.lock();
            switch (status)
            {
            case wsrep::provider::success:
                break;
            case wsrep::provider::error_bf_abort:
                if (state() != s_must_abort)
                {
                    state(lock, s_must_abort);
                }
                state(lock, s_must_replay);
                ret = 1;
                break;
            default:
                ret = 1;
                assert(0);
                break;
            }
        }
        break;
    case wsrep::client_state::m_high_priority:
        assert(certified());
        assert(ordered());
        if (client_service_.do_2pc() == false)
        {
            ret = before_prepare(lock) || after_prepare(lock);
        }
        else
        {
            ret = 0;
        }
        ret = ret || provider().commit_order_enter(ws_handle_, ws_meta_);
        if (ret)
        {
            state(lock, s_must_abort);
        }
        break;
    default:
        assert(0);
        break;
    }
    debug_log_state("before_commit_leave");
    return ret;
}

int wsrep::transaction::ordered_commit()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("ordered_commit_enter");
    assert(state() == s_committing);
    assert(ordered());
    int ret(provider().commit_order_leave(ws_handle_, ws_meta_));
    // Should always succeed
    assert(ret == 0);
    state(lock, s_ordered_commit);
    debug_log_state("ordered_commit_leave");
    return ret;
}

int wsrep::transaction::after_commit()
{
    int ret(0);

    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_commit_enter");
    assert(state() == s_ordered_commit);

    if (is_streaming())
    {
        assert(client_state_.mode() == wsrep::client_state::m_replicating ||
               client_state_.mode() == wsrep::client_state::m_high_priority);
        clear_fragments();
    }

    switch (client_state_.mode())
    {
    case wsrep::client_state::m_local:
        // Nothing to do
        break;
    case wsrep::client_state::m_replicating:
        ret = provider().release(ws_handle_);
        break;
    case wsrep::client_state::m_high_priority:
        break;
    default:
        assert(0);
        break;
    }
    assert(ret == 0);
    state(lock, s_committed);

    // client_state_.server_state().last_committed_gtid(ws_meta.gitd());
    debug_log_state("after_commit_leave");
    return ret;
}

int wsrep::transaction::before_rollback()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("before_rollback_enter");
    assert(state() == s_executing ||
           state() == s_must_abort ||
           state() == s_aborting || // Background rollbacker
           state() == s_cert_failed ||
           state() == s_must_replay);

    switch (state())
    {
    case s_executing:
        // Voluntary rollback
        if (is_streaming())
        {
            streaming_rollback();
        }
        state(lock, s_aborting);
        break;
    case s_must_abort:
        if (certified())
        {
            state(lock, s_must_replay);
        }
        else
        {
            if (is_streaming())
            {
                streaming_rollback();
            }
            state(lock, s_aborting);
        }
        break;
    case s_cert_failed:
        if (is_streaming())
        {
            streaming_rollback();
        }
        state(lock, s_aborting);
        break;
    case s_aborting:
        if (is_streaming())
        {
            provider().rollback(id_.get());
        }
        break;
    case s_must_replay:
        break;
    default:
        assert(0);
        break;
    }
    debug_log_state("before_rollback_leave");
    return 0;
}

int wsrep::transaction::after_rollback()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_rollback_enter");
    assert(state() == s_aborting ||
           state() == s_must_replay);

    if (state() == s_aborting)
    {
        state(lock, s_aborted);
    }

    // Releasing the transaction from provider is postponed into
    // after_statement() hook. Depending on DBMS system all the
    // resources acquired by transaction may or may not be released
    // during actual rollback. If the transaction has been ordered,
    // releasing the commit ordering critical section should be
    // also postponed until all resources have been released.
    debug_log_state("after_rollback_leave");
    return 0;
}

int wsrep::transaction::after_statement()
{
    int ret(0);
    wsrep::unique_lock<wsrep::mutex> lock(client_state_.mutex());
    debug_log_state("after_statement_enter");
    assert(state() == s_executing ||
           state() == s_committed ||
           state() == s_aborted ||
           state() == s_must_abort ||
           state() == s_cert_failed ||
           state() == s_must_replay);

    if (state() == s_executing &&
        streaming_context_.fragment_size() &&
        streaming_context_.fragment_unit() == streaming_context::statement)
    {
        streaming_context_.increment_unit_counter(1);
        if (streaming_context_.unit_counter() >= streaming_context_.fragment_size())
        {
            streaming_context_.reset_unit_counter();
            ret = certify_fragment(lock);
        }
    }

    switch (state())
    {
    case s_executing:
        // ?
        break;
    case s_committed:
        assert(is_streaming() == false);
        break;
    case s_must_abort:
    case s_cert_failed:
        client_state_.override_error(wsrep::e_deadlock_error);
        lock.unlock();
        ret = client_service_.rollback();
        lock.lock();
        if (state() != s_must_replay)
        {
            break;
        }
        // Continue to replay if rollback() changed the state to s_must_replay
        // Fall through
    case s_must_replay:
    {
        state(lock, s_replaying);
        lock.unlock();
        enum wsrep::provider::status replay_ret(client_service_.replay());
        switch (replay_ret)
        {
        case wsrep::provider::success:
            provider().release(ws_handle_);
            break;
        case wsrep::provider::error_certification_failed:
            client_state_.override_error(
                wsrep::e_deadlock_error);
            ret = 1;
            break;
        default:
            client_service_.emergency_shutdown();
            break;
        }
        lock.lock();
        if (ret)
        {
            wsrep::log_info() << "Replay ret " << replay_ret;
            state(lock, s_aborted);
        }
        break;
    }
    case s_aborted:
        break;
    default:
        assert(0);
        break;
    }

    assert(state() == s_executing ||
           state() == s_committed ||
           state() == s_aborted   ||
           state() == s_must_replay);

    if (state() == s_aborted)
    {
        if (ordered())
        {
            ret = provider().commit_order_enter(ws_handle_, ws_meta_);
            if (ret == 0)
            {
                // client_state_.server_state().last_committed_gtid(
                //   ws_meta.gtid());
                provider().commit_order_leave(ws_handle_, ws_meta_);
            }
        }
        provider().release(ws_handle_);
    }

    if (state() != s_executing)
    {
        cleanup();
    }

    debug_log_state("after_statement_leave");
    assert(ret == 0 || state() == s_aborted);
    return ret;
}

bool wsrep::transaction::bf_abort(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    wsrep::seqno bf_seqno)
{
    bool ret(false);
    assert(lock.owns_lock());

    if (active() == false)
    {
        WSREP_TC_LOG_DEBUG(1, "Transaction not active, skipping bf abort");
    }
    else if (ordered() && seqno() < bf_seqno)
    {
        WSREP_TC_LOG_DEBUG(1,
                           "Not allowed to BF abort transaction ordered before "
                           << "aborter: " << seqno() << " < " << bf_seqno);
    }
    else
    {
        switch (state())
        {
        case s_executing:
        case s_preparing:
        case s_certifying:
        case s_committing:
        {
            wsrep::seqno victim_seqno;
            enum wsrep::provider::status
                status(client_state_.provider().bf_abort(
                           bf_seqno, id_.get(), victim_seqno));
            switch (status)
            {
            case wsrep::provider::success:
                WSREP_TC_LOG_DEBUG(1, "Seqno " << bf_seqno
                                   << " succesfully BF aborted " << id_.get()
                                   << " victim_seqno " << victim_seqno);
                bf_abort_state_ = state();
                state(lock, s_must_abort);
                ret = true;
                break;
            default:
                WSREP_TC_LOG_DEBUG(1,
                                   "Seqno " << bf_seqno
                                   << " failed to BF abort " << id_.get()
                                   << " with status " << status
                                   << " victim_seqno " << victim_seqno);
                break;
            }
            break;
        }
        default:
            WSREP_TC_LOG_DEBUG(1, "BF abort not allowed in state "
                               << wsrep::to_string(state()));
            break;
        }
    }

    if (ret)
    {
        bf_abort_client_state_ = client_state_.state();
        if (client_state_.state() == wsrep::client_state::s_idle &&
            client_state_.server_state().rollback_mode() ==
            wsrep::server_state::rm_sync)
        {
            // We need to change the state to aborting under the
            // lock protection to avoid a race between client thread,
            // otherwise it could happend that the client gains control
            // between releasing the lock and before background
            // rollbacker gets control.
            state(lock, wsrep::transaction::s_aborting);
            lock.unlock();
            server_service_.background_rollback(client_state_);
        }
    }
    return ret;
}

////////////////////////////////////////////////////////////////////////////////
//                                 Private                                    //
////////////////////////////////////////////////////////////////////////////////

inline wsrep::provider& wsrep::transaction::provider()
{
    return client_state_.server_state().provider();
}

void wsrep::transaction::state(
    wsrep::unique_lock<wsrep::mutex>& lock __attribute__((unused)),
    enum wsrep::transaction::state next_state)
{
    if (client_state_.debug_log_level() >= 1)
    {
        log_debug() << "client: " << client_state_.id().get()
                    << " txc: " << id().get()
                    << " state: " << to_string(state_)
                    << " -> " << to_string(next_state);
    }
    assert(lock.owns_lock());
    static const char allowed[n_states][n_states] =
        { /*  ex pr ce co oc ct cf ma ab ad mr re */
            { 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0}, /* ex */
            { 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0}, /* pr */
            { 1, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0}, /* ce */
            { 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0}, /* co */
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}, /* oc */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ct */
            { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, /* cf */
            { 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0}, /* ma */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, /* ab */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ad */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* mr */
            { 0, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0}  /* re */
        };
    if (allowed[state_][next_state])
    {
        state_hist_.push_back(state_);
        state_ = next_state;
    }
    else
    {
        std::ostringstream os;
        os << "unallowed state transition for transaction "
           << id_.get() << ": " << wsrep::to_string(state_)
           << " -> " << wsrep::to_string(next_state);
        wsrep::log_error() << os.str();
        throw wsrep::runtime_error(os.str());
    }
}

int wsrep::transaction::certify_fragment(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());

    assert(client_state_.mode() == wsrep::client_state::m_replicating);
    assert(streaming_context_.rolled_back() == false);

    client_service_.wait_for_replayers(lock);
    if (state() == s_must_abort)
    {
        client_state_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_certifying);

    lock.unlock();

    wsrep::mutable_buffer data;
    if (client_service_.prepare_fragment_for_replication(data))
    {
        lock.lock();
        state(lock, s_must_abort);
        return 1;
    }

    // Client context to store fragment in separate transaction
    // Switch temporarily to sr_transaction, switch back
    // to original when this goes out of scope
    wsrep::scoped_client_state<wsrep::client_deleter> sr_client_state_scope(
        server_service_.local_client_state(),
        wsrep::client_deleter(server_service_));
    wsrep::client_state& sr_client_state(
        sr_client_state_scope.client_state());
    wsrep::client_state_switch client_state_switch(
        client_state_,
        sr_client_state);

    wsrep::unique_lock<wsrep::mutex> sr_lock(sr_client_state.mutex());
    wsrep::transaction& sr_transaction(
        sr_client_state.transaction_);
    sr_transaction.state(sr_lock, s_certifying);
    sr_lock.unlock();
    if (sr_client_state.client_service().append_fragment(
            sr_transaction, flags_,
            wsrep::const_buffer(data.data(), data.size())))
    {
        lock.lock();
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_append_fragment_error);
        return 1;
    }

    enum wsrep::provider::status
        cert_ret(provider().certify(client_state_.id().get(),
                                   sr_transaction.ws_handle_,
                                   flags_,
                                   sr_transaction.ws_meta_));

    int ret(0);
    switch (cert_ret)
    {
    case wsrep::provider::success:
        streaming_context_.certified(sr_transaction.ws_meta().seqno());
        sr_lock.lock();
        sr_transaction.certified_ = true;
        sr_transaction.state(sr_lock, s_committing);
        sr_lock.unlock();
        if (sr_client_state.client_service().commit(
                sr_transaction.ws_handle(), sr_transaction.ws_meta()))
        {
            ret = 1;
        }
        break;
    default:
        sr_lock.lock();
        sr_transaction.state(sr_lock, s_must_abort);
        sr_lock.unlock();
        sr_client_state.client_service().rollback();
        ret = 1;
        break;
    }
    lock.lock();
    if (ret)
    {
        state(lock, s_must_abort);
    }
    else
    {
        state(lock, s_executing);
        flags_ &= ~wsrep::provider::flag::start_transaction;
    }
    return ret;
}

int wsrep::transaction::certify_commit(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    assert(active());
    client_service_.wait_for_replayers(lock);

    assert(lock.owns_lock());

    if (state() == s_must_abort)
    {
        client_state_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_certifying);

    flags(flags() | wsrep::provider::flag::commit);

    lock.unlock();

    if (client_service_.prepare_data_for_replication())
    {
        // Note: Error must be set by prepare_data_for_replication()
        lock.lock();
        client_state_.override_error(wsrep::e_error_during_commit);
        state(lock, s_must_abort);
        return 1;
    }

    if (client_service_.interrupted())
    {
        lock.lock();
        client_state_.override_error(wsrep::e_interrupted_error);
        state(lock, s_must_abort);
        return 1;
    }

    client_service_.debug_sync("wsrep_before_certification");
    enum wsrep::provider::status
        cert_ret(provider().certify(client_state_.id().get(),
                                   ws_handle_,
                                   flags(),
                                   ws_meta_));
    client_service_.debug_sync("wsrep_after_certification");

    lock.lock();

    assert(state() == s_certifying || state() == s_must_abort);

    int ret(1);
    switch (cert_ret)
    {
    case wsrep::provider::success:
        assert(ordered());
        certified_ = true;
        switch (state())
        {
        case s_certifying:
            state(lock, s_committing);
            ret = 0;
            break;
        case s_must_abort:
            // We got BF aborted after succesful certification
            // and before acquiring client context lock. This means that
            // the trasaction must be replayed.
            client_service_.will_replay();
            state(lock, s_must_replay);
            break;
        default:
            assert(0);
            break;
        }
        break;
    case wsrep::provider::error_warning:
        assert(ordered() == false);
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_transaction_missing:
        state(lock, s_must_abort);
        // The execution should never reach this point if the
        // transaction has not generated any keys or data.
        wsrep::log_warning() << "Transaction was missing in provider";
        client_state_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_bf_abort:
        // Transaction was replicated succesfully and it was either
        // certified succesfully or the result of certifying is not
        // yet known. Therefore the transaction must roll back
        // and go through replay either to replay and commit the whole
        // transaction or to determine failed certification status.
        client_service_.will_replay();
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        state(lock, s_must_replay);
        break;
    case wsrep::provider::error_certification_failed:
        state(lock, s_cert_failed);
        client_state_.override_error(wsrep::e_deadlock_error);
        break;
    case wsrep::provider::error_size_exceeded:
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_connection_failed:
    case wsrep::provider::error_provider_failed:
        // Galera provider may return CONN_FAIL if the trx is
        // BF aborted O_o
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        client_state_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_fatal:
        client_state_.override_error(wsrep::e_error_during_commit);
        state(lock, s_must_abort);
        client_service_.emergency_shutdown();
        break;
    case wsrep::provider::error_not_implemented:
    case wsrep::provider::error_not_allowed:
        client_state_.override_error(wsrep::e_error_during_commit);
        state(lock, s_must_abort);
        wsrep::log_warning() << "Certification operation was not allowed: "
                             << "id: " << id().get()
                             << " flags: " << std::hex << flags() << std::dec;
        break;
    default:
        state(lock, s_must_abort);
        client_state_.override_error(wsrep::e_error_during_commit);
        break;
    }

    return ret;
}

void wsrep::transaction::streaming_rollback()
{
    assert(streaming_context_.rolled_back() == false);
    wsrep::client_state* sac(
        server_service_.streaming_applier_client_state());
    client_state_.server_state().start_streaming_applier(
        client_state_.server_state().id(), id(), sac);
    sac->adopt_transaction(*this);
    streaming_context_.cleanup();
    // Replicate rollback fragment
    provider().rollback(id_.get());
}

void wsrep::transaction::clear_fragments()
{
    streaming_context_.cleanup();
}

void wsrep::transaction::cleanup()
{
    assert(is_streaming() == false);
    assert(state() == s_committed || state() == s_aborted);
    debug_log_state("cleanup_enter");
    id_ = wsrep::transaction_id::undefined();
    ws_handle_ = wsrep::ws_handle();
    // Keep the state history for troubleshooting. Reset at start_transaction().
    // state_hist_.clear();
    ws_meta_ = wsrep::ws_meta();
    certified_ = false;
    pa_unsafe_ = false;
    client_service_.cleanup_transaction();
    debug_log_state("cleanup_leave");
}

void wsrep::transaction::debug_log_state(
    const char* context) const
{
    WSREP_TC_LOG_DEBUG(
        1, context
        << "(" << client_state_.id().get()
        << "," << int64_t(id_.get())
        << "," << ws_meta_.seqno().get()
        << "," << wsrep::to_string(state_)
        << ","
        << wsrep::to_string(client_state_.current_error())
        << ")");
}
