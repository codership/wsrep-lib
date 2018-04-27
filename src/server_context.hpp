//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/*! \file server_context.hpp
 *
 * Server Context Abstraction
 * ==========================
 *
 * This file defines an interface for TRRep Server Context.
 * The Server Context will encapsulate server identification,
 * server state and server capabilities. The class also
 * defines an interface for manipulating server state, applying
 * of remote transaction write sets, processing SST requests,
 * creating local client connections for local storage access
 * operations.
 *
 * Concepts
 * ========
 *
 * State Snapshot Transfer
 * -----------------------
 *
 * TODO
 *
 * Rollback Mode
 * -------------
 *
 * When High Prioity Transaction (HTP) write set is applied, it
 * may be required that the HTP Brute Force Aborts (BFA) locally
 * executing transaction. As HTP must be able to apply all its
 * write sets without interruption, the locally executing transaction
 * must yield immediately, otherwise a transaction processing
 * may stop or even deadlock. Depending on DBMS implementation,
 * the local transaction may need to be rolled back immediately
 * (synchronous mode)  or the rollback may happen later on
 * (asynchronous mode). The Server Context implementation
 * which derives from Server Context base class must provide
 * the base class the rollback mode which server operates on.
 *
 * ### Synchronous
 *
 * If the DBMS server implementation does not allow asynchronous rollback,
 * the victim transaction must be rolled back immediately in order to
 * allow transaction processing to proceed. Depending on DBMS process model,
 * there may be either background thread which processes the rollback
 * or the rollback can be done by the HTP applier.
 *
 * ### Asynchronous
 *
 * In asynchronous mode the BFA victim transaction is just marked
 * to be aborted or in case of fully optimistic concurrency control,
 * the conflict is detected at commit.
 */

#ifndef TRREP_SERVER_CONTEXT_HPP
#define TRREP_SERVER_CONTEXT_HPP

#include "exception.hpp"

#include "wsrep_api.h"

#include <string>

namespace trrep
{
    // Forward declarations
    class provider;
    class client_context;
    class transaction_context;
    class view;
    class data;

    /*! \class Server Context
     *
     * 
     */
    class server_context
    {
    public:

        /*! Rollback Mode enumeration
         *
         */
        enum rollback_mode
        {
            /*! Asynchronous rollback mode */
            rm_async,
            /*! Synchronous rollback mode */
            rm_sync
        };

        /*! Server Context constructor
         *
         * \param name Human Readable Server Name.
         * \param id Server Identifier String, UUID or some unique
         *        identifier.
         * \param address Server address in form of IPv4 address, IPv6 address
         *        or hostname.
         * \param working_dir Working directory for replication specific
         *        data files.
         * \param rollback_mode Rollback mode which server operates on.
         */
        server_context(const std::string& name,
                       const std::string& id,
                       const std::string& address,
                       const std::string& working_dir,
                       enum rollback_mode rollback_mode)
            : provider_()
            , name_(name)
            , id_(id)
            , address_(address)
            , working_dir_(working_dir)
            , rollback_mode_(rollback_mode)
        { }

        virtual ~server_context();
        /*!
         * Return human readable server name.
         *
         * \return Human readable server name string.
         */
        const std::string& name() const { return name_; }

        /*!
         * Return Server identifier string.
         *
         * \return Server indetifier string.
         */
        const std::string& id() const { return id_; }

        /*!
         * Return server group communication address.
         *
         * \return Return server group communication address.
         */
        const std::string& address() const { return address_; }

        /*!
         * Create client context which acts only locally, i.e. does
         * not participate in replication. However, local client
         * connection may execute transactions which require ordering,
         * as when modifying local SR fragment storage requires
         * strict commit ordering.
         *
         * \return Pointer to Client Context.
         */
        virtual client_context* local_client_context() = 0;

        /*!
         * Load WSRep provider.
         *
         * \param provider WSRep provider library to be loaded.
         * \param provider_options Provider specific options string
         *        to be passed for provider during initialization.
         *
         * \return Zero on success, non-zero on error.
         */
        int load_provider(const std::string& provider,
                          const std::string& provider_options);

        /*!
         * Return reference to provider.
         *
         * \return Reference to provider
         *
         * \throw trrep::runtime_error if provider has not been loaded
         */
        virtual trrep::provider& provider() const
        {
            if (provider_ == 0)
            {
                throw trrep::runtime_error("provider not loaded");
            }
            return *provider_;
        }

        /*!
         * Virtual method which will be called when the server
         * has been joined to the cluster. Must be provided by
         * the implementation.
         */
        virtual void on_connect() = 0;

        /*!
         * Wait until the server has connected to the cluster.
         *
         * \todo This should not be pure virtual method,
         * this base class should provide the server state
         * machine and proper synchronization.
         */
        virtual void wait_until_connected() = 0;

        /*!
         * Virtual method which will be called when a view
         * notification event has been delivered by the
         * provider.
         *
         * \params view trrep::view object which holds the new view
         *         information.
         */
        virtual void on_view(const trrep::view& view) = 0;

        /*!
         * Virtual method which will be called when the server
         * has been synchronized with the cluster.
         */
        virtual void on_sync() = 0;

        /*!
         * Virtual method which will be called on *joiner* when the provider
         * requests the SST request information. This method should
         * provide a string containing an information which the donor
         * server can use to donate SST.
         */
        virtual std::string on_sst_request() = 0;

        /*!
         * Virtual method which will be called on *donor* when the
         * SST request has been delivered by the provider.
         * This method should initiate SST transfer or throw
         * a trrep::runtime_error
         * if the SST transfer cannot be initiated. If the SST request
         * initiation is succesful, the server remains in s_donor
         * state until the SST is over or fails. The \param bypass
         * should be passed to SST implementation. If the flag is true,
         * no actual SST should happen, but the joiner server should
         * be notified that the donor has seen the request. The notification
         * should included \param gtid provided. This must be passed
         * to sst_received() call on the joiner.
         *
         *  \todo Figure out better exception for error codition.
         *
         * \param sst_request SST request string provided by the joiner.
         * \param gtid GTID denoting the current replication position.
         * \param bypass Boolean bypass flag.
         */
        virtual void on_sst_donate_request(const std::string& sst_request,
                                           const wsrep_gtid_t& gtid,
                                           bool bypass) = 0;

        /*!
         * This method must be called by the joiner after the SST
         * transfer has been received.
         *
         * \param gtid GTID provided by the SST transfer
         */
        void sst_received(const wsrep_gtid_t& gtid);

        /*!
         * This method will be called by the provider hen
         * a remote write set is being applied. It is the responsibility
         * of the caller to set up transaction context and data properly.
         *
         * \todo Make this private, allow calls for provider implementations
         *       only.
         * \param client_context Applier client context.
         * \param transaction_context Transaction context.
         * \param data Write set data
         *
         * \return Zero on success, non-zero on failure.
         */
        int on_apply(trrep::client_context& client_context,
                     trrep::transaction_context& transaction_context,
                     const trrep::data& data);

        /*!
         * This virtual method should be implemented by the DBMS
         * to provide information if the current statement in processing
         * is allowd for streaming replication.
         *
         * \return True if the statement is allowed for streaming
         *         replication, false otherwise.
         */
        virtual bool statement_allowed_for_streaming(
            const trrep::client_context& client_context,
            const trrep::transaction_context& transaction_context) const;
    private:

        server_context(const server_context&);
        server_context& operator=(const server_context&);

        trrep::provider* provider_;
        std::string name_;
        std::string id_;
        std::string address_;
        std::string working_dir_;
        enum rollback_mode rollback_mode_;
    };
}

#endif // TRREP_SERVER_CONTEXT_HPP
