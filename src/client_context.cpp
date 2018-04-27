//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "client_context.hpp"
#include "transaction_context.hpp"

trrep::provider& trrep::client_context::provider() const
{
    return server_context_.provider();
}

int trrep::client_context::before_command()
{
    assert(state_ == s_idle);
    if (server_context_.rollback_mode() == trrep::server_context::rm_sync)
    {

        /*!
         * \todo Wait until the possible synchronous rollback
         * has been finished.
         */
        trrep::unique_lock<trrep::mutex> lock(mutex_);
        while (transaction_.state() == trrep::transaction_context::s_aborting)
        {
            // cond_.wait(lock);
        }
    }
    state_ = s_exec;
    if (transaction_.state() == trrep::transaction_context::s_must_abort)
    {
        return 1;
    }
    return 0;
}

int trrep::client_context::after_command()
{
    int ret(0);
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    if (transaction_.state() == trrep::transaction_context::s_must_abort)
    {
        lock.unlock();
        rollback(transaction_);
        lock.lock();
        ret = 1;
    }
    state_ = s_idle;
    return ret;
}

int trrep::client_context::before_statement()
{
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
    return 0;
}

int trrep::client_context::after_statement()
{
#if 0
    /*!
     * \todo Check for replay state, do rollback if requested.
     */
#endif // 0
    return 0;
}

// TODO: This should be pure virtual method, implemented by
// DBMS integration or mock classes only.
int trrep::client_context::replay(
    trrep::unique_lock<trrep::mutex>& lock,
    trrep::transaction_context& tc)
{
    tc.state(lock, trrep::transaction_context::s_replaying);
    tc.state(lock, trrep::transaction_context::s_committed);
    return 0;
}
