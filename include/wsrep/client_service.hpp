//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/*! \file client_service.hpp
 *
 * This file will define a `callback` abstract interface for a
 * DBMS client session service. The interface will define methods
 * which will be called by the wsrep-lib under certain circumstances,
 * for example when a transaction rollback is required by internal
 * wsrep-lib operation or applier client needs to apply a write set.
 */

#include "buffer.hpp"

namespace wsrep
{
    class client_service
    {
    public:
        client_service(wsrep::provider& provider)
            : provider_(provider) { }
        /*!
         *
         */
        virtual bool is_autocommit() const = 0;

        /*!
         * Return true if two pahase commit is required for transaction
         * to commit.
         */
        virtual bool do_2pc() const = 0;

        /*!
         * Return true if the current transaction has been interrupted
         * by the DBMS.
         */
        virtual bool interrupted() const = 0;

        /*!
         * Reset possible global or thread local parameters associated
         * to the thread.
         */
        virtual void reset_globals() = 0;

        /*!
         * Store possible global or thread local parameters associated
         * to the thread.
         */
        virtual void store_globals() = 0;

        /*!
         * Set up a data for replication.
         */
        virtual int prepare_data_for_replication(wsrep::client_context&, const wsrep::transaction_context&) = 0;

        //
        // Streaming
        //
        virtual size_t bytes_generated() const = 0;
        virtual int prepare_fragment_for_replication(
            wsrep::client_context&, const wsrep::transaction_context&, wsrep::mutable_buffer&) = 0;
        virtual void remove_fragments(const wsrep::transaction_context&) = 0;

        /*!
         * Apply a write set.
         */
        virtual int apply(wsrep::client_context&, const wsrep::const_buffer&) = 0;

        /*!
         * Commit transaction.
         */
        virtual int commit(wsrep::client_context&, const wsrep::ws_handle&, const wsrep::ws_meta&) = 0;

        /*!
         * Roll back transaction.
         */
        virtual int rollback(wsrep::client_context&) = 0;

        /*!
         * Forcefully shut down the DBMS process or replication system.
         * This may be called in situations where
         * the process may encounter a situation where data integrity
         * may not be guaranteed or other unrecoverable condition is
         * encontered.
         */
        virtual void emergency_shutdown() = 0;

        // Replaying
        /*!
         * Notify that the client will replay.
         *
         * \todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual void will_replay(const wsrep::transaction_context&) = 0;

        /*!
         * Replay the current transaction. The implementation must put
         * the caller Client Context into applying mode and call
         * client_context::replay().
         *
         * \todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual enum wsrep::provider::status replay(wsrep::transaction_context&) = 0;

        /*!
         * Wait until all replaying transactions have been finished
         * replaying.
         *
         * \todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual void wait_for_replayers(wsrep::client_context&, wsrep::unique_lock<wsrep::mutex>&) = 0;

        // Streaming replication
        /*!
         * Append a write set fragment into fragment storage.
         */
        virtual int append_fragment(const wsrep::transaction_context&, int flag, const wsrep::const_buffer&) = 0;

        //
        // Debug interface
        //
        /*!
         * Enter debug sync point.
         *
         * @params sync_point Name of the debug sync point.
         */
        virtual void debug_sync(wsrep::client_context&, const char* sync_point) = 0;

        /*!
         * Forcefully kill the process if the crash_point has
         * been enabled.
         */
        virtual void debug_crash(const char* crash_point) = 0;

    protected:
        wsrep::provider& provider_;
    };

    /*!
     * Debug callback methods. These methods are called only in
     * builds that have WITH_DEBUG defined.
     */
    class client_debug_callback
    {
    public:
    };


}
