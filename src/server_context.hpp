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
 * Depending on SST type (physical or logical), the server storage
 * engine initialization must be done before or after SST happens.
 * In case of physical SST method (typically rsync, filesystem snapshot)
 * the SST happens before the storage engine is initialized, in case
 * of logical backup typically after the storage engine initialization.
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
 *
 */

#ifndef TRREP_SERVER_CONTEXT_HPP
#define TRREP_SERVER_CONTEXT_HPP

#include "exception.hpp"
#include "mutex.hpp"
#include "condition_variable.hpp"
#include "wsrep_api.h"

#include <string>
#include <vector>

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
        /*!
         * Server state enumeration.
         *
         * \todo Fix UML generation
         *
         * Server state diagram if the sst_before_init() returns false.
         *
         * [*] --> disconnected
         * disconnected --> initializing
         * initializing --> initialized
         * initialized --> connected
         * connected --> joiner
         * joiner --> joined
         * joined --> synced
         * synced --> donor
         * donor --> joined
         *
         * Server state diagram if the sst_before_init() returns true.
         *
         * [*] --> disconnected
         * disconnected --> connected
         * connected --> joiner
         * joiner --> initializing
         * initializing --> initialized
         * initialized --> joined
         * joined --> synced
         * synced --> donor
         * donor --> joined
         */
        enum state
        {
            /*! Server is in disconnected state. */
            s_disconnected,
            /*! Server is initializing */
            s_initializing,
            /*! Server has been initialized */
            s_initialized,
            /*! Server is connected to the cluster */
            s_connected,
            /*! Server is receiving SST */
            s_joiner,
            /*! Server has received SST succesfully but has not synced
              with rest of the cluster yet. */
            s_joined,
            /*! Server is donating state snapshot transfer */
            s_donor,
            /*! Server has synced with the cluster */
            s_synced,
            /*! Server is disconnecting from group */
            s_disconnecting
        };

        static const int n_states_ = s_disconnecting + 1;

        /*!
         * Rollback Mode enumeration
         */
        enum rollback_mode
        {
            /*! Asynchronous rollback mode */
            rm_async,
            /*! Synchronous rollback mode */
            rm_sync
        };


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
         * Get the rollback mode which server is operating in.
         *
         * \return Rollback mode.
         */
        enum rollback_mode rollback_mode() const { return rollback_mode_; }

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

        int connect(const std::string& cluster_name,
                    const std::string& cluster_address,
                    const std::string& state_donor,
                    bool bootstrap);

        int disconnect();
        /*!
         * Virtual method which will be called when the server
         * has been joined to the cluster. Must be provided by
         * the implementation.
         *
         * \todo Document overriding.
         */
        virtual void on_connect();

        /*!
         * Virtual method which will be called when a view
         * notification event has been delivered by the
         * provider.
         *
         * \todo Document overriding.
         *
         * \params view trrep::view object which holds the new view
         *         information.
         */
        virtual void on_view(const trrep::view& view);

        /*!
         * Virtual method which will be called when the server
         * has been synchronized with the cluster.
         *
         * \todo Document overriding.
         */
        virtual void on_sync();

        /*!
         * Wait until server reaches given state.
         */
        void wait_until_state(trrep::server_context::state) const;

        /*!
         * Virtual method to return true if the configured SST
         * method requires SST to be performed before DBMS storage
         * engine initialization, false otherwise.
         */
        virtual bool sst_before_init() const = 0;

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

    protected:
                /*! Server Context constructor
         *
         * \param mutex Mutex provided by the DBMS implementation.
         * \param name Human Readable Server Name.
         * \param id Server Identifier String, UUID or some unique
         *        identifier.
         * \param address Server address in form of IPv4 address, IPv6 address
         *        or hostname.
         * \param working_dir Working directory for replication specific
         *        data files.
         * \param rollback_mode Rollback mode which server operates on.
         */
        server_context(trrep::mutex& mutex,
                       trrep::condition_variable& cond,
                       const std::string& name,
                       const std::string& id,
                       const std::string& address,
                       const std::string& working_dir,
                       enum rollback_mode rollback_mode)
            : mutex_(mutex)
            , cond_(cond)
            , state_(s_disconnected)
            , state_waiters_(n_states_)
            , provider_()
            , name_(name)
            , id_(id)
            , address_(address)
            , working_dir_(working_dir)
            , rollback_mode_(rollback_mode)
        { }

    private:

        server_context(const server_context&);
        server_context& operator=(const server_context&);

        void state(trrep::unique_lock<trrep::mutex>&, enum state);

        trrep::mutex& mutex_;
        trrep::condition_variable& cond_;
        enum state state_;
        mutable std::vector<int> state_waiters_;
        trrep::provider* provider_;
        std::string name_;
        std::string id_;
        std::string address_;
        std::string working_dir_;
        enum rollback_mode rollback_mode_;
    };

    static inline std::string to_string(enum trrep::server_context::state state)
    {
        switch (state)
        {
        case trrep::server_context::s_disconnected:  return "disconnected";
        case trrep::server_context::s_initializing:  return "initilizing";
        case trrep::server_context::s_initialized:   return "initilized";
        case trrep::server_context::s_connected:     return "connected";
        case trrep::server_context::s_joiner:        return "joiner";
        case trrep::server_context::s_joined:        return "joined";
        case trrep::server_context::s_donor:         return "donor";
        case trrep::server_context::s_synced:        return "synced";
        case trrep::server_context::s_disconnecting: return "disconnecting";
        }
        return "unknown";
    }

}

#endif // TRREP_SERVER_CONTEXT_HPP
