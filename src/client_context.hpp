//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/*! \file client_context.hpp
 *
 * Client Context
 * ==============
 *
 * This file provides abstraction for integrating DBMS client
 * with replication system.
 *
 * Client Modes
 * ============
 *
 * Local
 * -----
 *
 * Replicating
 * -----------
 *
 * Applier
 * --------
 *
 * Client State
 * ============
 *
 * Client state is mainly relevant for the operation if the Server
 * supports synchronous rollback mode only. In this case the transactions
 * of the the idle clients (controls is in application which is using the
 * DBMS system) which encounter Brute Force Abort (BFA) must be rolled
 * back either by applier or a background process. If the client
 * state is executing, the control is inside the DBMS system and
 * the rollback process should be performed by the client which
 * drives the transaction.
 */

#ifndef TRREP_CLIENT_CONTEXT_HPP
#define TRREP_CLIENT_CONTEXT_HPP

#include "server_context.hpp"
#include "transaction_context.hpp"
#include "mutex.hpp"
#include "lock.hpp"
#include "data.hpp"

namespace trrep
{
    class server_context;
    class provider;

    enum client_error
    {
        e_success,
        e_error_during_commit,
        e_deadlock_error,
        e_append_fragment_error
    };

    class client_id
    {
    public:
        template <typename I>
        client_id(I id)
            : id_(static_cast<wsrep_conn_id_t>(id))
        { }
        wsrep_conn_id_t get() const { return id_; }
        static wsrep_conn_id_t invalid() { return -1; }
    private:
        wsrep_conn_id_t id_;
    };

    /*! \class Client Context
     *
     * Client Contex abstract interface.
     */
    class client_context
    {
    public:
        /*!
         * Client mode enumeration.
         * \todo m_toi total order isolation mode
         */
        enum mode
        {
            /*! Operates in local only mode, no replication. */
            m_local,
            /*! Generates write sets for replication by the provider. */
            m_replicating,
            /*! Applies write sets from the provider. */
            m_applier
        };

        /*!
         * Client state enumeration.
         *
         */
        enum state
        {
            /*!
             * Client is idle, the control is in the application which
             * uses the DBMS system.
             */
            s_idle,
            /*!
             * The control of the client processing is inside the DBMS
             * system.
             */
            s_exec,
            /*!
             * The client session is terminating.
             */
            s_quitting
        };


        virtual ~client_context() { }

        /*!
         * Get reference to the client mutex.
         *
         * \return Reference to the client mutex.
         */
        trrep::mutex& mutex() { return mutex_; }

        /*!
         * Get server context associated the the client session.
         *
         * \return Reference to server context.
         */
        trrep::server_context& server_context() const
        { return server_context_; }

        /*!
         * Get reference to the Provider which is associated
         * with the client context.
         *
         * \return Reference to the provider.
         * \throw trrep::runtime_error if no providers are associated
         *        with the client context.
         */
        trrep::provider& provider() const;

        /*!
         * Get Client identifier.
         *
         * \return Client Identifier
         */
        client_id id() const { return id_; }

        /*!
         * Get Client mode.
         *
         * \todo Enforce mutex protection if called from other threads.
         *
         * \return Client mode.
         */
        enum mode mode() const { return mode_; }

        /*!
         * Get Client state.
         *
         * \todo Enforce mutex protection if called from other threads.
         *
         * \return Client state
         */
        enum state state() const { return state_; }

        /*!
         * Virtual method to return true if the client operates
         * in two phase commit mode.
         *
         * \return True if two phase commit is required, false otherwise.
         */
        virtual bool do_2pc() const = 0;

        /*!
         * Virtual method which should be called before the client
         * starts processing the command received from the application.
         * This method will wait until the possible synchronous
         * rollback for associated transaction has finished.
         * The method has a side effect of changing the client
         * context state to executing.
         *
         * If overridden, the implementation should call base
         * class method before any implementation specific operations.
         *
         * \return Zero in case of success, non-zero in case of the
         *         associated transaction was BF aborted.
         */
        virtual int before_command();

        /*!
         * Virtual method which should be called before returning
         * the control back to application which uses the DBMS system.
         * This method will check if the transaction associated to
         * the connection has been aborted. This method has a side effect
         * of changing the client state to idle.
         *
         * If overridden, the implementation should call base
         * class metods after any implementation specifict operations.
         */
        virtual int after_command();

        /*!
         * Before statement execution operations.
         *
         * Check if server is synced and if dirty reads are allowed.
         *
         * If the method is overridden by the implementation, base class
         * method should be called before any implementation specifc
         * operations.
         *
         * \return Zero in case of success, non-zero if the statement
         *         is not allowed to be executed due to read or write
         *         isolation requirements.
         */
        virtual int before_statement();

        /*!
         * After statement execution operations.
         *
         * * Check for must_replay state
         * * Do rollback if requested
         *
         * If overridden by the implementation, base class method
         * should be called after any implementation specific operations.
         */
        virtual int after_statement();

        /*!
         * Append SR fragment to the transaction.
         */
        virtual int append_fragment(trrep::transaction_context&,
                                    uint32_t, const trrep::data&)
        { return 0; }

        /*!
         * Commit the transaction.
         */
        virtual int commit(trrep::transaction_context&) = 0;

        /*!
         * Rollback the transaction.
         */
        virtual int rollback(trrep::transaction_context&) = 0;

        virtual void will_replay(trrep::transaction_context&) { }
        virtual int replay(trrep::unique_lock<trrep::mutex>&,
                           trrep::transaction_context& tc);


        virtual int apply(trrep::transaction_context&,
                          const trrep::data&) = 0;

        virtual void wait_for_replayers(trrep::unique_lock<trrep::mutex>&)
        { }
        virtual int prepare_data_for_replication(
            const trrep::transaction_context&, trrep::data& data)
        {
            static const char buf[1] = { 1 };
            data.assign(buf, 1);
            return 0;
        }
        virtual void override_error(const trrep::client_error&) { }
        virtual bool killed() const { return 0; }
        virtual void abort() const { ::abort(); }
        virtual void store_globals() { }
        // Debug helpers
        virtual void debug_sync(const std::string&)
        {

        }
        virtual void debug_suicide(const std::string&)
        {
            abort();
        }
    protected:
        /*!
         * Client context constuctor
         */
        client_context(trrep::mutex& mutex,
                       trrep::server_context& server_context,
                       const client_id& id,
                       enum mode mode)
            : mutex_(mutex)
            , server_context_(server_context)
            , id_(id)
            , mode_(mode)
            , state_(s_idle)
            , transaction_(*this)
            , allow_dirty_reads_()
        { }

    private:
        void state(enum state state);

        trrep::mutex& mutex_;
        trrep::server_context& server_context_;
        client_id id_;
        enum mode mode_;
        enum state state_;
        trrep::transaction_context transaction_;
        /*!
         * \todo This boolean should be converted to better read isolation
         * semantics.
         */
        bool allow_dirty_reads_;
    };


    class client_context_switch
    {
    public:
        client_context_switch(trrep::client_context& orig_context,
                              trrep::client_context& current_context)
            : orig_context_(orig_context)
            , current_context_(current_context)
        {
            current_context_.store_globals();
        }
        ~client_context_switch()
        {
            orig_context_.store_globals();
        }
    private:
        client_context& orig_context_;
        client_context& current_context_;
    };
}

#endif // TRREP_CLIENT_CONTEXT_HPP
