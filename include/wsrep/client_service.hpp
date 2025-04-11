/*
 * Copyright (C) 2018-2025 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

/** @file client_service.hpp
 *
 * This file will define a `callback` abstract interface for a
 * DBMS client session service. The interface will define methods
 * which will be called by the wsrep-lib.
 */

#ifndef WSREP_CLIENT_SERVICE_HPP
#define WSREP_CLIENT_SERVICE_HPP

#include "buffer.hpp"
#include "provider.hpp"
#include "mutex.hpp"
#include "lock.hpp"

namespace wsrep
{
    class client_service
    {
    public:
        client_service() { }
        virtual ~client_service() { }

        /**
         * Return true if the current transaction has been interrupted
         * by the DBMS. The lock which is passed to interrupted call
         * will always have underlying mutex locked.
         *
         * @param lock Lock object grabbed by the client_state
         */
        virtual bool interrupted(wsrep::unique_lock<wsrep::mutex>& lock) const = 0;

        /**
         * Reset possible global or thread local parameters associated
         * to the thread.
         */
        virtual void reset_globals() = 0;

        /**
         * Store possible global or thread local parameters associated
         * to the thread.
         */
        virtual void store_globals() = 0;

        /**
         * Set up a data for replication.
         */
        virtual int prepare_data_for_replication() = 0;

        /**
         * Clean up after transcation has been terminated.
         */
        virtual void cleanup_transaction() = 0;

        //
        // Streaming
        //
        /**
         * Return true if current statement is allowed for streaming,
         * otherwise false.
         */
        virtual bool statement_allowed_for_streaming() const = 0;

        /**
         * Return the total number of bytes generated by the transaction
         * context.
         */
        virtual size_t bytes_generated() const = 0;

        /**
         * Prepare a buffer containing data for the next fragment to replicate.
         * The caller may set log_position to record the database specific
         * position corresponding to changes contained in the buffer.
         * When the call returns, the log_position will be available to read
         * from streaming_context::log_position().
         *
         * @return Zero in case of success, non-zero on failure.
         *         If there is no data to replicate, the method shall return
         *         zero and leave the buffer empty.
         */
        virtual int prepare_fragment_for_replication(wsrep::mutable_buffer& buffer,
                                                     size_t& log_position) = 0;

        /**
         * Remove fragments from the storage within current transaction.
         * Fragment removal will be committed once the current transaction
         * commits.
         *
         * @return Zero in case of success, non-zero on failure.
         */
        virtual int remove_fragments() = 0;

        //
        // Rollback
        //
        /**
         * Perform brute force rollback.
         *
         * This method may be called from two contexts, either from
         * client state methods when the BF abort condition is detected,
         * or from the background rollbacker thread. The task for this
         * method is to release all reasources held by the client
         * after BF abort so that the high priority thread can continue
         * applying.
         */
        virtual int bf_rollback() = 0;

        //
        // Interface to global server state
        //
        /**
         * Forcefully shut down the DBMS process or replication system.
         * This may be called in situations where
         * the process may encounter a situation where data integrity
         * may not be guaranteed or other unrecoverable condition is
         * encontered.
         */
        virtual void emergency_shutdown() = 0;

        // Replaying
        /**
         * Notify that the client will replay.
         *
         * @todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual void will_replay() = 0;

        /**
         * Signal that replay is done.
         */
        virtual void signal_replayed() = 0;

        /**
         * Replay the current transaction. The implementation must put
         * the caller Client Context into applying mode and call
         * client_state::replay().
         *
         * @todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual enum wsrep::provider::status replay() = 0;

        /**
         * Replay the current transaction. This is used for replaying
         * prepared XA transactions, which are BF aborted but not
         * while orderding commit / rollback.
         */
        virtual enum wsrep::provider::status replay_unordered() = 0;

        /**
         * Wait until all replaying transactions have been finished
         * replaying.
         *
         * @todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>&) = 0;

        //
        // XA
        //
        /**
         * Send a commit by xid
         */
        virtual enum wsrep::provider::status commit_by_xid() = 0;

        /**
         * Returns true if the client has an ongoing XA transaction.
         * This method is used to determine when to cleanup the
         * corresponding wsrep-lib transaction object.
         * This method should return false when the XA transaction
         * is over, and the wsrep-lib transaction object can be
         * cleaned up.
         */
        virtual bool is_explicit_xa() = 0;

        /**
         * Returns true if the client has an ongoing XA transaction
         * in prepared state.
         * Notice: one could simply check if wsrep::transaction is
         * in s_prepared state. However, wsrep::transaction does not
         * transition to prepared state for read-only / empty
         * transactions.
         */
        virtual bool is_prepared_xa() = 0;

        /**
         * Returns true if the currently executing command is
         * a rollback for XA. This is used to avoid setting a
         * a deadlock error rollback as it may be unexpected
         * by the DBMS.
         */
        virtual bool is_xa_rollback() = 0;

        //
        // Debug interface
        //
        /**
         * Enter debug sync point.
         *
         * @params sync_point Name of the debug sync point.
         */
        virtual void debug_sync(const char* sync_point) = 0;

        /**
         * Forcefully kill the process if the crash_point has
         * been enabled.
         */
        virtual void debug_crash(const char* crash_point) = 0;

        //
        // Notify state change interface
        //
        virtual void notify_state_change() = 0;
    };

}

#endif // WSREP_CLIENT_SERVICE_HPP
