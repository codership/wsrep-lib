//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "trrep/client_context.hpp"
#include "trrep/compiler.hpp"

#include <sstream>
#include <iostream>


trrep::provider& trrep::client_context::provider() const
{
    return server_context_.provider();
}

int trrep::client_context::before_command()
{
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    assert(state_ == s_idle);
    if (server_context_.rollback_mode() == trrep::server_context::rm_sync)
    {

        /*!
         * \todo Wait until the possible synchronous rollback
         * has been finished.
         */
        while (transaction_.state() == trrep::transaction_context::s_aborting)
        {
            // cond_.wait(lock);
        }
    }
    state(lock, s_exec);
    if (transaction_.active() &&
        (transaction_.state() == trrep::transaction_context::s_must_abort ||
         transaction_.state() == trrep::transaction_context::s_aborted))
    {
        return 1;
    }
    return 0;
}

void trrep::client_context::after_command_before_result()
{
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    assert(state() == s_exec);
    if (transaction_.active() &&
        transaction_.state() == trrep::transaction_context::s_must_abort)
    {
        override_error(trrep::e_deadlock_error);
        lock.unlock();
        rollback();
        transaction_.after_statement();
        lock.lock();
        assert(transaction_.state() == trrep::transaction_context::s_aborted);
        assert(current_error() != trrep::e_success);
    }
    state(lock, s_result);
}

void trrep::client_context::after_command_after_result()
{
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    assert(state() == s_result);
    if (transaction_.active() &&
        transaction_.state() == trrep::transaction_context::s_must_abort)
    {
        // Note: Error is not overridden here as the result has already
        // been sent to client. The error should be set in before_command()
        // when the client issues next command.
        lock.unlock();
        rollback();
        transaction_.after_statement();
        lock.lock();
        assert(transaction_.state() == trrep::transaction_context::s_aborted);
        assert(current_error() != trrep::e_success);
    }
    state(lock, s_idle);
}

int trrep::client_context::before_statement()
{
    trrep::unique_lock<trrep::mutex> lock(mutex_);
#if 0
    /*!
     * \todo It might be beneficial to implement timed wait for
     *       server synced state.
     */
    if (allow_dirty_reads_ == false &&
        server_context_.state() != trrep::server_context::s_synced)
    {
        return 1;
    }
#endif // 0

    if (transaction_.active() &&
        transaction_.state() == trrep::transaction_context::s_must_abort)
    {
        override_error(trrep::e_deadlock_error);
        lock.unlock();
        rollback();
        lock.lock();
        return 1;
    }
    return 0;
}

void trrep::client_context::after_statement()
{
#if 0
    /*!
     * \todo Check for replay state, do rollback if requested.
     */
#endif // 0
    (void)transaction_.after_statement();
}

// Private

void trrep::client_context::state(
    trrep::unique_lock<trrep::mutex>& lock TRREP_UNUSED,
    enum trrep::client_context::state state)
{
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
        throw trrep::runtime_error(os.str());
    }
}
