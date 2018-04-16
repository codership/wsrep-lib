//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "transaction_context.hpp"
#include "client_context.hpp"
#include "server_context.hpp"
#include "key.hpp"
#include "data.hpp"

#include <iostream> // TODO: replace with proper logging utility
#include <sstream>

// Public

int trrep::transaction_context::append_key(const trrep::key& key)
{

    return provider_.append_key(&ws_handle_, &key.get());
}

int trrep::transaction_context::append_data(const trrep::data& data)
{

    return provider_.append_data(&ws_handle_, &data.get());
}

int trrep::transaction_context::before_prepare()
{
    int ret(0);

    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());

    assert(client_context_.mode() == trrep::client_context::m_local);
    assert(state() == s_executing || state() == s_must_abort);

    if (state() == s_must_abort)
    {
        return 1;
    }

    if (is_streaming())
    {
        client_context_.debug_suicide(
            "crash_last_fragment_commit_before_fragment_removal");
        lock.unlock();
        if (client_context_.server_context().statement_allowed_for_streaming(
                client_context_, *this))
        {
            client_context_.override_error(trrep::e_error_during_commit);
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
    assert(client_context_.mode() == trrep::client_context::m_local);
    assert(state() == s_executing);
    return ret;
}

int trrep::transaction_context::after_prepare()
{
    int ret(1);
    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());

    assert(client_context_.mode() == trrep::client_context::m_local);
    assert(state() == s_executing || state() == s_must_abort);
    if (state() == s_must_abort)
    {
        return 1;
    }

    if (state() == s_executing)
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
        client_context_.override_error(trrep::e_deadlock_error);
    }
    assert(client_context_.mode() == trrep::client_context::m_local);
    return ret;
}

int trrep::transaction_context::before_commit()
{
    int ret(1);

    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());
    switch (client_context_.mode())
    {
    case trrep::client_context::m_local:

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
                client_context_.override_error(trrep::e_deadlock_error);
        }
        else
        {
            // 2PC commit, prepare was done before
            ret = 0;
        }
        if (ret == 0)
        {
            lock.unlock();
            switch(provider_.commit_order_enter(&ws_handle_))
            {
            case WSREP_OK:
                break;
            case WSREP_BF_ABORT:
                state(lock, s_must_abort);
                ret = 1;
                break;
            default:
                ret = 1;
                assert(0);
                break;
            }
            lock.lock();
        }

        break;
    case trrep::client_context::m_applier:
        ret = provider_.commit_order_enter(&ws_handle_);
        if (ret)
        {
            state(lock, s_must_abort);
        }
        break;
    }
    return ret;
}

int trrep::transaction_context::ordered_commit()
{
    int ret(1);

    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());
    assert(state() == s_committing);
    ret = provider_.commit_order_leave(&ws_handle_);
    // Should always succeed
    assert(ret == 0);
    state(lock, s_ordered_commit);
    return ret;
}

int trrep::transaction_context::after_commit()
{
    int ret(0);

    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());

    assert(state() == s_ordered_commit);

    switch (client_context_.mode())
    {
    case trrep::client_context::m_local:
        if (is_streaming())
        {
            clear_fragments();
        }
        ret = provider_.release(&ws_handle_);
        break;
    case trrep::client_context::m_applier:
        break;
    }
    assert(ret == 0);
    state(lock, s_committed);
    return ret;
}

int trrep::transaction_context::before_rollback()
{
    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());

    assert(state() == s_executing ||
           state() == s_must_abort ||
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
    case s_must_replay:
        break;
    default:
        assert(0);
        break;
    }
    return 0;
}

int trrep::transaction_context::after_rollback()
{
    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());

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
    return 0;
}

int trrep::transaction_context::after_statement()
{
    int ret(0);
    trrep::unique_lock<trrep::mutex> lock(client_context_.mutex());

    assert(state() == s_executing ||
           state() == s_committed ||
           state() == s_aborted ||
           state() == s_must_abort ||
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
        ret = client_context_.rollback(*this);
        break;
    case s_aborted:
        break;
    case s_must_replay:
        ret = client_context_.replay(*this);
        break;
    default:
        assert(0);
        break;
    }

    assert(state() == s_executing ||
           state() == s_committed ||
           state() == s_aborted);

    if (state() == s_aborted)
    {
        if (ordered())
        {
            ret = provider_.commit_order_enter(&ws_handle_);
            if (ret == 0) provider_.commit_order_leave(&ws_handle_);
        }
        provider_.release(&ws_handle_);
    }

    if (state() != s_executing)
    {
        cleanup();
    }

    return ret;
}

// Private

void trrep::transaction_context::state(
    trrep::unique_lock<trrep::mutex>& lock __attribute__((unused)),
    enum trrep::transaction_context::state next_state)
{
    assert(lock.owns_lock());
    static const char allowed[n_states][n_states] =
        { /*  ex ce co oc ct cf ma ab ad mr re from/to */
            { 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0}, /* ex */
            { 0, 0, 1, 0, 0, 1, 1, 0, 0, 0, 0}, /* ce */
            { 0, 0, 0, 1, 1, 0, 1, 0, 0, 0, 0}, /* co */
            { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}, /* oc */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ct */
            { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0}, /* cf */
            { 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0}, /* ma */
            { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0}, /* ab */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, /* ad */
            { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}, /* mr */
            { 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0}  /* re */
        };
    if (allowed[state_][next_state])
    {
        std::cerr << "state transition: " << trrep::to_string(state_)
                  << " -> " << trrep::to_string(next_state)
                  << "\n";
        state_hist_.push_back(state_);
        state_ = next_state;
    }
    else
    {
        std::ostringstream os;
        os << "unallowed state transition for transaction "
           << id_.get() << ": " << trrep::to_string(state_)
           << " -> " << trrep::to_string(next_state);
        std::cerr << os.str() << "\n";
        throw trrep::runtime_error(os.str());
    }
}

int trrep::transaction_context::certify_fragment()
{
    throw trrep::not_implemented_error();
}

int trrep::transaction_context::certify_commit(
    trrep::unique_lock<trrep::mutex>& lock)
{
    assert(lock.owns_lock());
    assert(id_ != trrep::transaction_id::invalid());

    client_context_.wait_for_replayers(lock);

    assert(lock.owns_lock());

    if (state() == s_must_abort)
    {
        client_context_.override_error(trrep::e_deadlock_error);
        return 1;
    }

    state(lock, s_certifying);

    lock.unlock();

    trrep::data data;
    if (client_context_.prepare_data_for_replication(*this, data))
    {
        // Note: Error must be set by prepare_data_for_replication()
        lock.lock();
        state(lock, s_must_abort);
        return 1;
    }

    if (client_context_.killed())
    {
        lock.lock();
        client_context_.override_error(trrep::e_deadlock_error);
        state(lock, s_must_abort);
        return 1;
    }

    wsrep_status cert_ret(provider_.certify_commit(client_context_.id().get(),
                                                   &ws_handle_,
                                                   flags() | WSREP_FLAG_TRX_END,
                                                   &trx_meta_));

    lock.lock();

    assert(state() == s_certifying || state() == s_must_abort);
    client_context_.debug_sync("wsrep_after_replication");

    int ret(1);
    switch (cert_ret)
    {
    case WSREP_OK:
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
    case WSREP_WARNING:
        assert(ordered() == false);
        state(lock, s_must_abort);
        client_context_.override_error(trrep::e_error_during_commit);
        break;
    case WSREP_TRX_MISSING:
        state(lock, s_must_abort);
        // The execution should never reach this point if the
        // transaction has not generated any keys or data.
        client_context_.override_error(trrep::e_error_during_commit);
        assert(0);
        break;
    case WSREP_BF_ABORT:
        // Transaction was replicated succesfully and it was either
        // certified succesfully or the result of certifying is not
        // yet known. Therefore the transaction must roll back
        // and go through replay either to replay and commit the whole
        // transaction or to determine failed certification status.
        assert(ordered());
        client_context_.will_replay(*this);
        state(lock, s_must_abort);
        state(lock, s_must_replay);
        break;
    case WSREP_TRX_FAIL:
        state(lock, s_cert_failed);
        client_context_.override_error(trrep::e_deadlock_error);
        break;
    case WSREP_SIZE_EXCEEDED:
        state(lock, s_must_abort);
        client_context_.override_error(trrep::e_error_during_commit);
        break;
    case WSREP_CONN_FAIL:
    case WSREP_NODE_FAIL:
        state(lock, s_must_abort);
        client_context_.override_error(trrep::e_error_during_commit);
        break;
    case WSREP_FATAL:
        client_context_.abort();
        break;
    case WSREP_NOT_IMPLEMENTED:
    case WSREP_NOT_ALLOWED:
        client_context_.override_error(trrep::e_error_during_commit);
        state(lock, s_must_abort);
        assert(0);
        break;
    default:
        client_context_.override_error(trrep::e_error_during_commit);
        break;
    }

    return ret;
}

void trrep::transaction_context::remove_fragments()
{
    throw trrep::not_implemented_error();
}

void trrep::transaction_context::clear_fragments()
{
    throw trrep::not_implemented_error();
}

void trrep::transaction_context::cleanup()
{
    std::cerr << "Cleanup transaction "
              << client_context_.id().get()
              << ": " << id_.get() << "\n";
    id_ = trrep::transaction_id::invalid();
    state_ = s_executing;
    state_hist_.clear();
    trx_meta_.gtid = WSREP_GTID_UNDEFINED;
    trx_meta_.stid.node = WSREP_UUID_UNDEFINED;
    trx_meta_.stid.trx = trrep::transaction_id::invalid();
    trx_meta_.stid.conn = trrep::client_id::invalid();
    certified_ = false;
    pa_unsafe_ = false;
}
