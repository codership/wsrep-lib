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

        const static int state_max_ = s_quitting + 1;
        /*!
         * Destructor.
         */
        virtual ~client_context()
        {
            assert(transaction_.active() == false);
        }

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

        int start_transaction(const trrep::transaction_id& id)
        {
            assert(state_ == s_exec);
            return transaction_.start_transaction(id);
        }

        int start_transaction(const wsrep_ws_handle_t& wsh,
                              const wsrep_trx_meta_t& meta,
                              uint32_t flags)
        {
            assert(mode_ == m_applier);
            return transaction_.start_transaction(wsh, meta, flags);
        }

        int append_key(const trrep::key& key)
        {
            assert(state_ == s_exec);
            return transaction_.append_key(key);
        }

        int append_data(const trrep::data& data)
        {
            assert(state_ == s_exec);
            return transaction_.append_data(data);
        }
        int before_prepare()
        {
            assert(state_ == s_exec);
            return transaction_.before_prepare();
        }

        int after_prepare()
        {
            assert(state_ == s_exec);
            return transaction_.after_prepare();
        }

        int before_commit()
        {
            assert(state_ == s_exec);
            return transaction_.before_commit();
        }

        int ordered_commit()
        {
            assert(state_ == s_exec);
            return transaction_.ordered_commit();
        }

        int after_commit()
        {
            assert(state_ == s_exec);
            return transaction_.after_commit();
        }
        int before_rollback()
        {
            assert(state_ == s_exec);
            return transaction_.before_rollback();
        }
        int after_rollback()
        {
            assert(state_ == s_exec);
            return transaction_.after_rollback();
        }
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

        trrep::transaction_context& transaction()
        {
            return transaction_;
        }
    protected:
        /*!
         * Client context constuctor. This is protected so that it
         * can be called from derived class constructors only.
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
        client_context(const client_context&);
        client_context& operator=(client_context&);

        /*
         * Friend declarations
         */
        friend int server_context::on_apply(client_context&,
                                            trrep::transaction_context&,
                                            const trrep::data&);
        friend class client_context_switch;
        friend class transaction_context;

        /*!
         * Get Client state.
         *
         * \todo Enforce mutex protection if called from other threads.
         *
         * \return Client state
         */
        enum state state() const { return state_; }

        /*!
         * Set client state.
         */
        void state(trrep::unique_lock<trrep::mutex>& lock, enum state state);

        /*!
         * Virtual method to return true if the client operates
         * in two phase commit mode.
         *
         * \return True if two phase commit is required, false otherwise.
         */
        virtual bool do_2pc() const = 0;


        /*!
         * Append SR fragment to the transaction.
         */
        virtual int append_fragment(trrep::transaction_context&,
                                    uint32_t, const trrep::data&)
        { return 0; }


        /*!
         * This method applies a write set give in data buffer.
         * This must be implemented by the DBMS integration.
         *
         * \return Zero on success, non-zero on applying failure.
         */
        virtual int apply(trrep::transaction_context& transaction,
                          const trrep::data& data) = 0;

        /*!
         * Virtual method which will be called
         * in order to commit the transaction into
         * storage engine.
         *
         * \return Zero on success, non-zero on failure.
         */
        virtual int commit(trrep::transaction_context&) = 0;

        /*!
         * Rollback the transaction.
         *
         * This metod must be implemented by DBMS integration.
         *
         * \return Zero on success, no-zero on failure.
         */
        virtual int rollback(trrep::transaction_context&) = 0;

        /*!
         * Notify a implementation that the client is about
         * to replay the transaction.
         */
        virtual void will_replay(trrep::transaction_context&) = 0;

        /*!
         * Replay the transaction.
         */
        virtual int replay(trrep::unique_lock<trrep::mutex>&,
                           trrep::transaction_context& tc) = 0;


        /*!
         * Wait until all of the replaying transactions have been committed.
         */
        virtual void wait_for_replayers(trrep::unique_lock<trrep::mutex>&) const = 0;

        virtual int prepare_data_for_replication(
            const trrep::transaction_context&, trrep::data& data)
        {
            static const char buf[1] = { 1 };
            data.assign(buf, 1);
            return 0;
        }

        /*!
         *
         */
        virtual void override_error(const trrep::client_error&) = 0;

        /*!
         * Return true if the current client operation was killed.
         */
        virtual bool killed() const = 0;

        /*!
         * Abort server operation on fatal error. This should be used
         * only for critical conditions which would sacrifice data
         * consistency.
         */
        virtual void abort() const = 0;

        /*!
         * Set up thread global variables for client connection.
         */
        virtual void store_globals() = 0;

        /*!
         * Enter debug synchronization point.
         */
        virtual void debug_sync(const std::string&) = 0;

        /*!
         *
         */
        virtual void debug_suicide(const std::string&) = 0;


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
