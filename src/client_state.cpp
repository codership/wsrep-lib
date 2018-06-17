//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/client_state.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/logger.hpp"

#include <sstream>
#include <iostream>


wsrep::provider& wsrep::client_state::provider() const
{
    return server_state_.provider();
}

void wsrep::client_state::override_error(enum wsrep::client_error error)
{
    assert(wsrep::this_thread::get_id() == thread_id_);
    if (current_error_ != wsrep::e_success &&
        error == wsrep::e_success)
    {
        throw wsrep::runtime_error("Overriding error with success");
    }
    current_error_ = error;
}


int wsrep::client_state::before_command()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("before_command: enter");
    assert(state_ == s_idle);
    if (server_state_.rollback_mode() == wsrep::server_state::rm_sync)
    {
        /**
         * @todo Wait until the possible synchronous rollback
         * has been finished.
         */
        while (transaction_.state() == wsrep::transaction::s_aborting)
        {
            // cond_.wait(lock);
        }
    }
    state(lock, s_exec);
    assert(transaction_.active() == false ||
           (transaction_.state() == wsrep::transaction::s_executing ||
            transaction_.state() == wsrep::transaction::s_aborted ||
            (transaction_.state() == wsrep::transaction::s_must_abort &&
             server_state_.rollback_mode() == wsrep::server_state::rm_async)));

    if (transaction_.active())
    {
        if (transaction_.state() == wsrep::transaction::s_must_abort)
        {
            assert(server_state_.rollback_mode() ==
                   wsrep::server_state::rm_async);
            override_error(wsrep::e_deadlock_error);
            lock.unlock();
            client_service_.rollback(*this);
            (void)transaction_.after_statement();
            lock.lock();
            assert(transaction_.state() ==
                   wsrep::transaction::s_aborted);
            assert(transaction_.active() == false);
            assert(current_error() != wsrep::e_success);
            debug_log_state("before_command: error");
            return 1;
        }
        else if (transaction_.state() == wsrep::transaction::s_aborted)
        {
            // Transaction was rolled back either just before sending result
            // to the client, or after client_state become idle.
            // Clean up the transaction and return error.
            override_error(wsrep::e_deadlock_error);
            lock.unlock();
            (void)transaction_.after_statement();
            lock.lock();
            assert(transaction_.active() == false);
            debug_log_state("before_command: error");
            return 1;
        }
    }
    debug_log_state("before_command: success");
    return 0;
}

void wsrep::client_state::after_command_before_result()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("after_command_before_result: enter");
    assert(state() == s_exec);
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        override_error(wsrep::e_deadlock_error);
        lock.unlock();
        client_service_.rollback(*this);
        (void)transaction_.after_statement();
        lock.lock();
        assert(transaction_.state() == wsrep::transaction::s_aborted);
        assert(current_error() != wsrep::e_success);
    }
    state(lock, s_result);
    debug_log_state("after_command_before_result: leave");
}

void wsrep::client_state::after_command_after_result()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("after_command_after_result_enter");
    assert(state() == s_result);
    assert(transaction_.state() != wsrep::transaction::s_aborting);
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        lock.unlock();
        client_service_.rollback(*this);
        lock.lock();
        assert(transaction_.state() == wsrep::transaction::s_aborted);
        override_error(wsrep::e_deadlock_error);
    }
    else if (transaction_.active() == false)
    {
        current_error_ = wsrep::e_success;
    }
    state(lock, s_idle);
    debug_log_state("after_command_after_result: leave");
}

int wsrep::client_state::before_statement()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("before_statement: enter");
#if 0
    /**
     * @todo It might be beneficial to implement timed wait for
     *       server synced state.
     */
    if (allow_dirty_reads_ == false &&
        server_state_.state() != wsrep::server_state::s_synced)
    {
        return 1;
    }
#endif // 0

    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction::s_must_abort)
    {
        // Rollback and cleanup will happen in after_command_before_result()
        debug_log_state("before_statement_error");
        return 1;
    }
    debug_log_state("before_statement: success");
    return 0;
}

enum wsrep::client_state::after_statement_result
wsrep::client_state::after_statement()
{
    // wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    debug_log_state("after_statement: enter");
    assert(state() == s_exec);
#if 0
    /**
     * @todo Check for replay state, do rollback if requested.
     */
#endif // 0
    (void)transaction_.after_statement();
    if (current_error() == wsrep::e_deadlock_error)
    {
        if (client_service_.is_autocommit())
        {
            debug_log_state("after_statement: may_retry");
            return asr_may_retry;
        }
        else
        {
            debug_log_state("after_statement: error");
            return asr_error;
        }
    }
    debug_log_state("after_statement: success");
    return asr_success;
}

int wsrep::client_state::enable_streaming(
    enum wsrep::streaming_context::fragment_unit
    fragment_unit,
    size_t fragment_size)
{
    assert(mode_ == m_replicating);
    if (transaction_.active() &&
        transaction_.streaming_context_.fragment_unit() !=
        fragment_unit)
    {
        wsrep::log_error()
            << "Changing fragment unit for active transaction "
            << "not allowed";
        return 1;
    }
    transaction_.streaming_context_.enable(
        fragment_unit, fragment_size);
    return 0;
}

// Private

void wsrep::client_state::debug_log_state(const char* context) const
{
    if (debug_log_level() >= 1)
    {
        wsrep::log_debug() << "client_state: " << context
                           << ": server: " << server_state_.name()
                           << " client: " << id_.get()
                           << " current_error: " << current_error_;
    }
}

void wsrep::client_state::state(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum wsrep::client_state::state state)
{
    assert(wsrep::this_thread::get_id() == thread_id_);
    assert(lock.owns_lock());
    static const char allowed[state_max_][state_max_] =
        {
            /* idle exec result quit */
            {  0,   1,   0,     1}, /* idle */
            {  0,   0,   1,     0}, /* exec */
            {  1,   0,   0,     1}, /* result */
            {  0,   0,   0,     0}  /* quit */
        };
    if (allowed[state_][state])
    {
        state_ = state;
    }
    else
    {
        std::ostringstream os;
        os << "client_state: Unallowed state transition: "
           << state_ << " -> " << state;
        throw wsrep::runtime_error(os.str());
    }
}
