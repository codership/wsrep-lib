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
 *
 * \todo Figure out better name for this interface.
 */

namespace wsrep
{
    class client_service
    {
    public:
        virtual bool is_autocommit() const = 0;
        /*!
         * Return true if two pahase commit is required for transaction
         * to commit.
         */
        virtual bool do_2pc() const = 0;

        /*!
         * Return true if the current transaction has been interrupted.
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
        virtual int prepare_data_for_replication(wsrep::data& data) = 0;


        /*!
         * Apply a write set.
         */
        virtual int apply() = 0;

        /*!
         * Prepare a transaction for commit.
         */
        virtual int prepare() = 0;

        /*!
         * Commit transaction.
         */
        virtual int commit() = 0;

        /*!
         * Roll back transaction.
         */
        virtual int rollback() = 0;

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
        virtual void will_replay() = 0;

        /*!
         * Replay the current transaction. The implementation must put
         * the caller Client Context into applying mode and call
         * client_context::replay().
         *
         * \todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual int replay() = 0;

        /*!
         * Wait until all replaying transactions have been finished
         * replaying.
         *
         * \todo This should not be visible to DBMS level, should be
         * handled internally by wsrep-lib.
         */
        virtual void wait_for_replayers() = 0;

        // Streaming replication
        /*!
         * Append a write set fragment into fragment storage.
         */
        virtual int append_fragment() = 0;
    };

    /*!
     * Debug callback methods. These methods are called only in
     * builds that have WITH_DEBUG defined.
     */
    class client_debug_callback
    {
    public:
        /*!
         * Enter debug sync point.
         *
         * @params sync_point Name of the debug sync point.
         */
        void debug_sync(const char* sync_point) = 0;

        /*!
         * Forcefully kill the process if the suicide_point has
         * been enabled.
         */
        void debug_suicide(const char* suicide_point) = 0;
    };


}
