//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction_context.hpp"
#include "wsrep/client_context.hpp"
#include "wsrep/server_context.hpp"
#include "wsrep/key.hpp"
#include "wsrep/data.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"

#include <sstream>
#include <memory>

// Public

wsrep::transaction_context::transaction_context(
    wsrep::client_context& client_context)
    : provider_(client_context.provider())
    , client_context_(client_context)
    , id_(transaction_id::invalid())
    , state_(s_executing)
    , state_hist_()
    , bf_abort_state_(s_executing)
    , bf_abort_client_state_()
    , ws_handle_()
    , ws_meta_()
    , flags_()
    , pa_unsafe_(false)
    , certified_(false)
    , fragments_()
    , rollback_replicated_for_(false)
{ }


wsrep::transaction_context::~transaction_context()
{
}

int wsrep::transaction_context::start_transaction(
    const wsrep::transaction_id& id)
{
    assert(active() == false);
    id_ = id;
    state_ = s_executing;
    state_hist_.clear();
    ws_handle_ = wsrep::ws_handle(id);
    flags_ |= wsrep::provider::flag::start_transaction;
    switch (client_context_.mode())
    {
    case wsrep::client_context::m_local:
    case wsrep::client_context::m_applier:
        return 0;
    case wsrep::client_context::m_replicating:
        return provider_.start_transaction(ws_handle_);
    default:
        assert(0);
        return 1;
    }
}

int wsrep::transaction_context::start_transaction(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    assert(ws_meta.flags());
    assert(active() == false);
    assert(client_context_.mode() == wsrep::client_context::m_applier);
    state_ = s_executing;
    ws_handle_ = ws_handle;
    ws_meta_ = ws_meta;
    certified_ = true;
    return 0;
}


int wsrep::transaction_context::append_key(const wsrep::key& key)
{

    return provider_.append_key(ws_handle_, key);
}

int wsrep::transaction_context::append_data(const wsrep::data& data)
{

    return provider_.append_data(ws_handle_, data);
}

int wsrep::transaction_context::before_prepare()
{
    int ret(0);

    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
    debug_log_state("before_prepare_enter");
    assert(state() == s_executing || state() == s_must_abort);

    if (state() == s_must_abort)
    {
        assert(client_context_.mode() == wsrep::client_context::m_replicating);
        client_context_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_preparing);

    switch (client_context_.mode())
    {
    case wsrep::client_context::m_replicating:
        if (is_streaming())
        {
            client_context_.debug_suicide(
                "crash_last_fragment_commit_before_fragment_removal");
            lock.unlock();
            if (client_context_.server_context().statement_allowed_for_streaming(
                    client_context_, *this))
            {
                client_context_.override_error(wsrep::e_error_during_commit);
                ret = 1;
            }
            else
            {
                remove_fragments();
            }
            lock.lock();
            client_context_.debug_suicide(
                "crash_last_fragment_commit_after_fragment_removal");
        }
        break;
    case wsrep::client_context::m_local:
    case wsrep::client_context::m_applier:
        break;
    default:
        assert(0);
        break;
    }

    assert(state() == s_preparing);
    debug_log_state("before_prepare_leave");
    return ret;
}

int wsrep::transaction_context::after_prepare()
{
    int ret(1);
    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
    debug_log_state("after_prepare_enter");
    assert(state() == s_preparing || state() == s_must_abort);
    if (state() == s_must_abort)
    {
        assert(client_context_.mode() == wsrep::client_context::m_replicating);
        client_context_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    switch (client_context_.mode())
    {
    case wsrep::client_context::m_replicating:
        if (state() == s_preparing)
        {
            ret = certify_commit(lock);
            assert((ret == 0 || state() == s_committing) ||
                   (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed));
        }
        else
        {
            assert(state() == s_must_abort);
            client_context_.override_error(wsrep::e_deadlock_error);
        }
        break;
    case wsrep::client_context::m_local:
    case wsrep::client_context::m_applier:
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

int wsrep::transaction_context::before_commit()
{
    int ret(1);

    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
    debug_log_state("before_commit_enter");
    assert(client_context_.mode() != wsrep::client_context::m_toi);
    assert(state() == s_executing ||
           state() == s_committing ||
           state() == s_must_abort ||
           state() == s_replaying);
    assert(state() != s_committing || certified());

    switch (client_context_.mode())
    {
    case wsrep::client_context::m_local:
        if (ordered())
        {
            ret = provider_.commit_order_enter(ws_handle_, ws_meta_);
        }
        break;
    case wsrep::client_context::m_replicating:

        // Commit is one phase - before/after prepare was not called
        if (state() == s_executing)
        {
            ret = certify_commit(lock);
            assert((ret == 0 || state() == s_committing)
                   ||
                   (state() == s_must_abort ||
                    state() == s_must_replay ||
                    state() == s_cert_failed));
        }
        else if (state() != s_committing)
        {
                assert(state() == s_must_abort);
                client_context_.override_error(wsrep::e_deadlock_error);
        }
        else
        {
            // 2PC commit, prepare was done before
            ret = 0;
        }
        if (ret == 0)
        {
            lock.unlock();
            enum wsrep::provider::status
                status(provider_.commit_order_enter(
                           ws_handle_, ws_meta_));
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
                ret = 1;
                break;
            default:
                ret = 1;
                assert(0);
                break;
            }

        }
        break;
    case wsrep::client_context::m_applier:
        assert(ordered());
        ret = provider_.commit_order_enter(ws_handle_, ws_meta_);
        if (ret)
        {
            state(lock, s_must_abort);
        }
        else
        {
            if (state() == s_executing)
            {
                // 1pc
                state(lock, s_certifying);
                state(lock, s_committing);
            }
            else if (state() == s_replaying)
            {
                state(lock, s_committing);
            }
        }
        break;
    default:
        assert(0);
        break;
    }
    debug_log_state("before_commit_leave");
    return ret;
}

int wsrep::transaction_context::ordered_commit()
{
    int ret(1);

    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
    debug_log_state("ordered_commit_enter");
    assert(state() == s_committing);
    assert(ordered());
    ret = provider_.commit_order_leave(ws_handle_, ws_meta_);
    // Should always succeed
    assert(ret == 0);
    state(lock, s_ordered_commit);
    debug_log_state("ordered_commit_leave");
    return ret;
}

int wsrep::transaction_context::after_commit()
{
    int ret(0);

    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
    debug_log_state("after_commit_enter");
    assert(state() == s_ordered_commit);

    switch (client_context_.mode())
    {
    case wsrep::client_context::m_local:
        // Nothing to do
        break;
    case wsrep::client_context::m_replicating:
        if (is_streaming())
        {
            clear_fragments();
        }
        ret = provider_.release(ws_handle_);
        break;
    case wsrep::client_context::m_applier:
        break;
    default:
        assert(0);
        break;
    }
    assert(ret == 0);
    state(lock, s_committed);
    debug_log_state("after_commit_leave");
    return ret;
}

int wsrep::transaction_context::before_rollback()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
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
            // Replicate rollback fragment
            provider_.rollback(id_.get());
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
                // Replicate rollback fragment
                provider_.rollback(id_.get());
            }
            state(lock, s_aborting);
        }
        break;
    case s_cert_failed:
        if (is_streaming())
        {
            // Replicate rollback fragment
            provider_.rollback(id_.get());
        }
        state(lock, s_aborting);
        break;
    case s_aborting:
        if (is_streaming())
        {
            provider_.rollback(id_.get());
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

int wsrep::transaction_context::after_rollback()
{
    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
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

int wsrep::transaction_context::after_statement()
{
    int ret(0);
    wsrep::unique_lock<wsrep::mutex> lock(client_context_.mutex());
    debug_log_state("after_statement_enter");
    assert(state() == s_executing ||
           state() == s_committed ||
           state() == s_aborted ||
           state() == s_must_abort ||
           state() == s_cert_failed ||
           state() == s_must_replay);

    switch (state())
    {
    case s_executing:
        // ?
        break;
    case s_committed:
        if (is_streaming())
        {
            state(lock, s_executing);
        }
        break;
    case s_must_abort:
    case s_cert_failed:
        client_context_.override_error(wsrep::e_deadlock_error);
        lock.unlock();
        ret = client_context_.rollback();
        lock.lock();
        if (state() != s_must_replay)
        {
            break;
        }
        // Continue to replay if rollback() changed the state to s_must_replay
        // Fall through
    case s_must_replay:
        state(lock, s_replaying);
        lock.unlock();
        ret = client_context_.replay(*this);
        lock.lock();
        provider_.release(ws_handle_);
        break;
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
            ret = provider_.commit_order_enter(ws_handle_, ws_meta_);
            if (ret == 0) provider_.commit_order_leave(ws_handle_, ws_meta_);
        }
        provider_.release(ws_handle_);
    }

    if (state() != s_executing)
    {
        cleanup();
    }

    debug_log_state("after_statement_leave");
    assert(ret == 0 || state() == s_aborted);
    return ret;
}

bool wsrep::transaction_context::bf_abort(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    wsrep::seqno bf_seqno)
{
    bool ret(false);
    assert(lock.owns_lock());
    assert(&lock.mutex() == &mutex());

    if (active() == false)
    {
        wsrep::log() << "Transaction not active, skipping bf abort";
    }
    else if (ordered() && seqno() < bf_seqno)
    {
        wsrep::log() << "Not allowed to BF abort transaction ordered before "
                     << "aborter: " << seqno() << " < " << bf_seqno;
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
                status(client_context_.provider().bf_abort(
                           bf_seqno, id_.get(), victim_seqno));
            switch (status)
            {
            case wsrep::provider::success:
                wsrep::log() << "Seqno " << bf_seqno
                             << " succesfully BF aborted " << id_.get()
                             << " victim_seqno " << victim_seqno;
                bf_abort_state_ = state();
                state(lock, s_must_abort);
                ret = true;
                break;
            default:
                wsrep::log() << "Seqno " << bf_seqno
                             << " failed to BF abort " << id_.get()
                             << " with status " << status
                             << " victim_seqno " << victim_seqno;
                break;
            }
            break;
        }
        default:
            wsrep::log() << "BF abort not allowed in state "
                         << wsrep::to_string(state());
            break;
        }
    }

    if (ret)
    {
        bf_abort_client_state_ = client_context_.state();
        if (client_context_.state() == wsrep::client_context::s_idle &&
            client_context_.server_context().rollback_mode() ==
            wsrep::server_context::rm_sync)
        {
            // We need to change the state to aborting under the
            // lock protection to avoid a race between client thread,
            // otherwise it could happend that the client gains control
            // between releasing the lock and before background
            // rollbacker gets control.
            state(lock, wsrep::transaction_context::s_aborting);
            lock.unlock();
            client_context_.server_context().background_rollback(client_context_);
        }
    }
    return ret;
}

wsrep::mutex& wsrep::transaction_context::mutex()
{
    return client_context_.mutex();
}
// Private

void wsrep::transaction_context::state(
    wsrep::unique_lock<wsrep::mutex>& lock __attribute__((unused)),
    enum wsrep::transaction_context::state next_state)
{
    if (client_context_.debug_log_level() >= 1)
    {
        log_debug() << "client: " << client_context_.id().get()
                    << " txc: " << id().get()
                    << " state: " << state_ << " -> " << next_state;
    }
    assert(lock.owns_lock());
    static const char allowed[n_states][n_states] =
        { /*  ex pr ce co oc ct cf ma ab ad mr re */
            { 0, 1, 1, 0, 0, 0, 0, 1, 1, 0, 0, 0}, /* ex */
            { 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0}, /* pr */
            { 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0}, /* ce */
            { 0, 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0}, /* co */
            { 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}, /* oc */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ct */
            { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, /* cf */
            { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0}, /* ma */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, /* ab */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ad */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* mr */
            { 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0}  /* re */
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
        wsrep::log() << os.str();
        throw wsrep::runtime_error(os.str());
    }
}

#if 0
int wsrep::transaction_context::certify_fragment(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    // This method is not fully implemented and tested yet.
    throw wsrep::not_implemented_error();
    assert(lock.owns_lock());

    assert(client_context_.mode() == wsrep::client_context::m_replicating);
    assert(rollback_replicated_for_ != id_);

    client_context_.wait_for_replayers(lock);
    if (state() == s_must_abort)
    {
        client_context_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_certifying);

    lock.unlock();

    uint32_t flags(0);
    if (fragments_.empty())
    {
        flags |= wsrep::provider::flag::start_transaction;
    }

    wsrep::data data;
    if (client_context_.prepare_data_for_replication(*this, data))
    {
        lock.lock();
        state(lock, s_must_abort);
        return 1;
    }

    // Client context to store fragment in separate transaction
    // Switch temporarily to sr_transaction_context, switch back
    // to original when this goes out of scope
    std::auto_ptr<wsrep::client_context> sr_client_context(
        client_context_.server_context().local_client_context());
    wsrep::client_context_switch client_context_switch(
        client_context_,
        *sr_client_context);
    wsrep::transaction_context sr_transaction_context(*sr_client_context);
    if (sr_client_context->append_fragment(sr_transaction_context, flags, data))
    {
        lock.lock();
        state(lock, s_must_abort);
        client_context_.override_error(wsrep::e_append_fragment_error);
        return 1;
    }

    enum wsrep::provider::status
        cert_ret(provider_.certify(client_context_.id().get(),
                                   &sr_transaction_context.ws_handle_,
                                   flags,
                                   &sr_transaction_context.trx_meta_));
    int ret(0);
    switch (cert_ret)
    {
    case wsrep::provider::success:
        sr_client_context->commit(sr_transaction_context);
        break;
    default:
        sr_client_context->rollback(sr_transaction_context);
        ret = 1;
        break;
    }

    return ret;
}
#endif

int wsrep::transaction_context::certify_commit(
    wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    assert(active());

    client_context_.wait_for_replayers(lock);

    assert(lock.owns_lock());

    if (state() == s_must_abort)
    {
        client_context_.override_error(wsrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_certifying);

    flags(flags() | wsrep::provider::flag::commit);
    lock.unlock();

    wsrep::data data;
    if (client_context_.prepare_data_for_replication(*this, data))
    {
        // Note: Error must be set by prepare_data_for_replication()
        lock.lock();
        client_context_.override_error(wsrep::e_error_during_commit);
        state(lock, s_must_abort);
        return 1;
    }

    if (client_context_.killed())
    {
        lock.lock();
        client_context_.override_error(wsrep::e_interrupted_error);
        state(lock, s_must_abort);
        return 1;
    }

    enum wsrep::provider::status
        cert_ret(provider_.certify(client_context_.id().get(),
                                   ws_handle_,
                                   flags(),
                                   ws_meta_));

    lock.lock();

    assert(state() == s_certifying || state() == s_must_abort);
    client_context_.debug_sync("wsrep_after_replication");

    int ret(1);
    switch (cert_ret)
    {
    case wsrep::provider::success:
        assert(ordered());
        switch (state())
        {
        case s_certifying:
            certified_ = true;
            state(lock, s_committing);
            ret = 0;
            break;
        case s_must_abort:
            // We got BF aborted after succesful certification
            // and before acquiring client context lock. This means that
            // the trasaction must be replayed.
            client_context_.will_replay(*this);
            state(lock, s_must_replay);
            break;
        default:
            assert(0);
            break;
        }
        certified_ = true;
        break;
    case wsrep::provider::error_warning:
        assert(ordered() == false);
        state(lock, s_must_abort);
        client_context_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_transaction_missing:
        state(lock, s_must_abort);
        // The execution should never reach this point if the
        // transaction has not generated any keys or data.
        client_context_.override_error(wsrep::e_error_during_commit);
        assert(0);
        break;
    case wsrep::provider::error_bf_abort:
        // Transaction was replicated succesfully and it was either
        // certified succesfully or the result of certifying is not
        // yet known. Therefore the transaction must roll back
        // and go through replay either to replay and commit the whole
        // transaction or to determine failed certification status.
        assert(ordered());
        client_context_.will_replay(*this);
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        state(lock, s_must_replay);
        break;
    case wsrep::provider::error_certification_failed:
        state(lock, s_cert_failed);
        client_context_.override_error(wsrep::e_deadlock_error);
        break;
    case wsrep::provider::error_size_exceeded:
        state(lock, s_must_abort);
        client_context_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_connection_failed:
    case wsrep::provider::error_provider_failed:
        // Galera provider may return CONN_FAIL if the trx is
        // BF aborted O_o
        if (state() != s_must_abort)
        {
            state(lock, s_must_abort);
        }
        client_context_.override_error(wsrep::e_error_during_commit);
        break;
    case wsrep::provider::error_fatal:
        client_context_.abort();
        break;
    case wsrep::provider::error_not_implemented:
    case wsrep::provider::error_not_allowed:
        client_context_.override_error(wsrep::e_error_during_commit);
        state(lock, s_must_abort);
        assert(0);
        break;
    default:
        client_context_.override_error(wsrep::e_error_during_commit);
        break;
    }

    return ret;
}

void wsrep::transaction_context::remove_fragments()
{
    throw wsrep::not_implemented_error();
}

void wsrep::transaction_context::clear_fragments()
{
    throw wsrep::not_implemented_error();
}

void wsrep::transaction_context::cleanup()
{
    debug_log_state("cleanup_enter");
    id_ = wsrep::transaction_id::invalid();
    ws_handle_ = wsrep::ws_handle();
    if (is_streaming())
    {
        state_ = s_executing;
    }
    // Keep the state history for troubleshooting. Reset at start_transaction().
    // state_hist_.clear();
    ws_meta_ = wsrep::ws_meta();
    certified_ = false;
    pa_unsafe_ = false;
    debug_log_state("cleanup_leave");
}

void wsrep::transaction_context::debug_log_state(
    const char* context) const
{
    if (client_context_.debug_log_level() >= 1)
    {
        wsrep::log_debug() << context
                           << ": server: " << client_context_.server_context().name()
                           << ": client: " << client_context_.id().get()
                           << " trx: " << int64_t(id_.get())
                           << " state: " << wsrep::to_string(state_)
                           << " error: "
                           << wsrep::to_string(client_context_.current_error());
    }
}
