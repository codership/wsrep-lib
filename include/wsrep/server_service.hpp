//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//


/** @file server_service.hpp
 *
 * An abstract interface for a DBMS server services.
 * The interface will define methods which will be called from
 * the wsrep-lib.
 */

#ifndef WSREP_SERVER_SERVICE_HPP
#define WSREP_SERVER_SERVICE_HPP

#include "logger.hpp"
#include "server_state.hpp"

#include <string>

namespace wsrep
{
    class client_service;
    class storage_service;
    class high_priority_service;
    class ws_meta;
    class gtid;
    class view;
    class server_service
    {
    public:

        virtual wsrep::storage_service* storage_service(
            wsrep::client_service&) = 0;

        virtual void release_storage_service(wsrep::storage_service*) = 0;

        /**
         * Create an applier state for streaming transaction applying.
         *
         * @param orig_cs  Reference to client service which is
         *                 requesting a new streaming applier service
         *                 instance.
         *
         * @return Pointer to streaming applier client state.
         */
        virtual wsrep::high_priority_service*
        streaming_applier_service(wsrep::client_service& orig_cs) = 0;

        /**
         * Create an applier state for streaming transaction applying.
         *
         * @param orig_hps Reference to high priority service which is
         *                 requesting a new streaming applier service
         *                 instance.
         *
         * @return Pointer to streaming applier client state.
         */
        virtual wsrep::high_priority_service*
        streaming_applier_service(wsrep::high_priority_service& orig_hps) = 0;

        /**
         * Release a client state allocated by either local_client_state()
         * or streaming_applier_client_state().
         */
        virtual void release_high_priority_service(
            wsrep::high_priority_service*) = 0;

        /**
         * Perform a background rollback for a transaction.
         */
        virtual void background_rollback(wsrep::client_state&) = 0;

        /**
         * Bootstrap a DBMS state for a new cluster.
         *
         * This method is called by the wsrep lib after the
         * new cluster is bootstrapped and the server has reached
         * initialized state. From this call the DBMS should initialize
         * environment for the new cluster.
         */
        virtual void bootstrap() = 0;

        /**
         * Log message
         *
         * @param level   Requested logging level
         * @param message Message
         */
        virtual void log_message(enum wsrep::log::level level,
                                 const char* message) = 0;
        /**
         * Log a dummy write set. A dummy write set is usually either
         * a remotely generated write set which failed certification in
         * provider and had a GTID assigned or a streaming replication
         * rollback write set. If the DBMS implements logging for
         * applied transactions, logging dummy write sets which do not
         * commit any transaction is neeeded to keep the GTID sequence
         * continuous in the server.
         */
        virtual void log_dummy_write_set(wsrep::client_state& client_state,
                                         const wsrep::ws_meta& ws_meta) = 0;

        /**
         * Log a cluster view change event.
         */
        virtual void log_view(const wsrep::view&) = 0;

        /**
         * Log a state change event.
         *
         * Note that this method may be called with server_state
         * mutex locked, so calling server_state public methods
         * should be avoided from within this call.
         *
         * @param prev_state Previous state server was in
         * @param current_state Current state
         */
        virtual void log_state_change(
            enum wsrep::server_state::state prev_state,
            enum wsrep::server_state::state current_state) = 0;

        /**
         * Determine if the configured SST method requires SST to be
         * performed before DBMS storage engine initialization.
         *
         * @return True if the SST must happen before storage engine init,
         *         otherwise false.
         */
        virtual bool sst_before_init() const = 0;

        /**
         * Return SST request which provides the donor server enough
         * information how to donate the snapshot.
         *
         * @return A string containing a SST request.
         */
        virtual std::string sst_request() = 0;

        /**
         * Start a SST process.
         *
         * @param sst_request A string containing the SST request from
         *                   the joiner
         * @param gtid A GTID denoting the current replication position
         * @param bypass Boolean bypass flag.
         *
         * @return Zero if the SST transfer was succesfully started,
         *         non-zero otherwise.
         */
        virtual int start_sst(const std::string& sst_request,
                              const wsrep::gtid& gtid,
                              bool bypass) = 0;


        /**
         * Wait until committing transactions have completed.
         * Prior calling this method the server should have been
         * desynced from the group to disallow further transactions
         * to start committing.
         */
        virtual int wait_committing_transactions(int timeout) = 0;

        /**
         * Provide a server level debug sync point for a caller.
         */
        virtual void debug_sync(const char* sync_point) = 0;
    };
}

#endif // WSREP_SERVER_SERVICE_HPP
