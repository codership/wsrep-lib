//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/client_context.hpp"
#include "wsrep/compiler.hpp"

#include <sstream>
#include <iostream>


wsrep::provider& wsrep::client_context::provider() const
{
    return server_context_.provider();
}

void wsrep::client_context::override_error(enum wsrep::client_error error)
{
    assert(wsrep::this_thread::get_id() == thread_id_);
    if (current_error_ != wsrep::e_success &&
        error == wsrep::e_success)
    {
        throw wsrep::runtime_error("Overriding error with success");
    }
    current_error_ = error;
}


int wsrep::client_context::before_command()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state_ == s_idle);
    if (server_context_.rollback_mode() == wsrep::server_context::rm_sync)
    {
        /*!
         * \todo Wait until the possible synchronous rollback
         * has been finished.
         */
        while (transaction_.state() == wsrep::transaction_context::s_aborting)
        {
            // cond_.wait(lock);
        }
    }
    state(lock, s_exec);
    assert(transaction_.active() == false ||
           (transaction_.state() == wsrep::transaction_context::s_executing ||
            transaction_.state() == wsrep::transaction_context::s_aborted));

    // Transaction was rolled back either just before sending result
    // to the client, or after client_context become idle.
    // Clean up the transaction and return error.
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction_context::s_aborted)
    {
        override_error(wsrep::e_deadlock_error);
        lock.unlock();
        (void)transaction_.after_statement();
        lock.lock();
        return 1;
    }
    return 0;
}

void wsrep::client_context::after_command_before_result()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state() == s_exec);
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction_context::s_must_abort)
    {
        override_error(wsrep::e_deadlock_error);
        lock.unlock();
        rollback();
        (void)transaction_.after_statement();
        lock.lock();
        assert(transaction_.state() == wsrep::transaction_context::s_aborted);
        assert(current_error() != wsrep::e_success);
    }
    state(lock, s_result);
}

void wsrep::client_context::after_command_after_result()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state() == s_result);
    assert(transaction_.state() != wsrep::transaction_context::s_aborting);
    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction_context::s_must_abort)
    {
        lock.unlock();
        rollback();
        lock.lock();
        assert(transaction_.state() == wsrep::transaction_context::s_aborted);
        override_error(wsrep::e_deadlock_error);
    }
    else if (transaction_.active() == false)
    {
        current_error_ = wsrep::e_success;
    }
    state(lock, s_idle);
}

int wsrep::client_context::before_statement()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
#if 0
    /*!
     * \todo It might be beneficial to implement timed wait for
     *       server synced state.
     */
    if (allow_dirty_reads_ == false &&
        server_context_.state() != wsrep::server_context::s_synced)
    {
        return 1;
    }
#endif // 0

    if (transaction_.active() &&
        transaction_.state() == wsrep::transaction_context::s_must_abort)
    {
        override_error(wsrep::e_deadlock_error);
        lock.unlock();
        rollback();
        lock.lock();
        return 1;
    }
    return 0;
}

enum wsrep::client_context::after_statement_result
wsrep::client_context::after_statement()
{
    // wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state() == s_exec);
#if 0
    /*!
     * \todo Check for replay state, do rollback if requested.
     */
#endif // 0
    int ret(transaction_.after_statement());
    if (ret)
    {
        if (is_autocommit())
        {
            return asr_may_retry;
        }
        else
        {
            return asr_error;
        }
    }
    return asr_success;
}

// Private

void wsrep::client_context::state(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum wsrep::client_context::state state)
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
        os << "client_context: Unallowed state transition: "
           << state_ << " -> " << state << "\n";
        throw wsrep::runtime_error(os.str());
    }
}
