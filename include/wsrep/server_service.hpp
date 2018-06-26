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

#include <string>

namespace wsrep
{
    class client_state;
    class ws_meta;
    class gtid;
    class view;
    class server_service
    {
    public:
        /**
         * Create client state instance which acts only locally, i.e. does
         * not participate in replication. However, local client
         * state may execute transactions which require ordering,
         * as when modifying local SR fragment storage requires
         * strict commit ordering.
         *
         * @return Pointer to Client State.
         */
        virtual wsrep::client_state* local_client_state() = 0;

        /**
         * Create an applier state for streaming transaction applying.
         *
         * @return Pointer to streaming applier client state.
         */
        virtual wsrep::client_state* streaming_applier_client_state() = 0;

        /**
         * Release a client state allocated by either local_client_state()
         * or streaming_applier_client_state().
         */
        virtual void release_client_state(wsrep::client_state*) = 0;

        /**
         * Perform a background rollback for a transaction.
         */
        virtual void background_rollback(wsrep::client_state&) = 0;

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


    };
}

#endif // WSREP_SERVER_SERVICE_HPP
