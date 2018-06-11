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

#ifndef WSREP_CLIENT_CONTEXT_HPP
#define WSREP_CLIENT_CONTEXT_HPP

#include "server_context.hpp"
#include "transaction_context.hpp"
#include "client_id.hpp"
#include "mutex.hpp"
#include "lock.hpp"
#include "data.hpp"
#include "thread.hpp"

namespace wsrep
{
    class server_context;
    class provider;

    enum client_error
    {
        e_success,
        e_error_during_commit,
        e_deadlock_error,
        e_interrupted_error,
        e_append_fragment_error
    };

    static inline std::string to_string(enum client_error error)
    {
        switch (error)
        {
        case e_success:               return "success";
        case e_error_during_commit:   return "error_during_commit";
        case e_deadlock_error:        return "deadlock_error";
        case e_interrupted_error:     return "interrupted_error";
        case e_append_fragment_error: return "append_fragment_error";
        }
        return "unknown";
    }

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
            m_applier,
            /*! Client is in total order isolation mode */
            m_toi
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
             * Client handler is sending result to client.
             */
            s_result,
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
         * Method which should be called before the client
         * starts processing the command received from the application.
         * This method will wait until the possible synchronous
         * rollback for associated transaction has finished.
         * The method has a side effect of changing the client
         * context state to executing.
         *
         * \return Zero in case of success, non-zero in case of the
         *         associated transaction was BF aborted.
         */
        int before_command();

        /*!
         * Method which should be called before returning
         * the control back to application which uses the DBMS system.
         * This method will check if the transaction associated to
         * the connection has been aborted. Rollback is performed
         * if needed.
         */
        void after_command_before_result();

        /*!
         * Method which should be called after returning the
         * control back to application which uses the DBMS system.
         * The method will do the check if the transaction associated
         * to the connection has been aborted. If so, rollback is
         * performed and the transaction is left to aborted state
         * so that the client will get appropriate error on next
         * command.
         *
         * This method has a side effect of changing state to
         * idle.
         */
        void after_command_after_result();

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
        int before_statement();

        /*!
         * Return values for after_statement() method.
         */
        enum after_statement_result
        {
            /*! Statement was executed succesfully */
            asr_success,
            /*! Statement execution encountered an error, the transaction
             * was rolled back */
            asr_error,
            /*! Statement execution encountered an error, the transaction
              was rolled back. However the statement was self contained
              (e.g. autocommit statement) so it can be retried. */
            asr_may_retry
        };
        /*!
         * After statement execution operations.
         *
         * * Check for must_replay state
         * * Do rollback if requested
         *
         * If overridden by the implementation, base class method
         * should be called after any implementation specific operations.
         */
        enum after_statement_result after_statement();

        int start_transaction()
        {
            assert(state_ == s_exec);
            return transaction_.start_transaction();
        }

        int start_transaction(const wsrep::transaction_id& id)
        {
            assert(state_ == s_exec);
            return transaction_.start_transaction(id);
        }

        int start_transaction(const wsrep::ws_handle& wsh,
                              const wsrep::ws_meta& meta)
        {
            assert(mode_ == m_applier);
            return transaction_.start_transaction(wsh, meta);
        }

        int append_key(const wsrep::key& key)
        {
            assert(state_ == s_exec);
            return transaction_.append_key(key);
        }

        int append_data(const wsrep::data& data)
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
            assert(state_ == s_idle || state_ == s_exec || state_ == s_result);
            return transaction_.before_rollback();
        }
        int after_rollback()
        {
            assert(state_ == s_idle || state_ == s_exec || state_ == s_result);
            return transaction_.after_rollback();
        }

        int bf_abort(wsrep::unique_lock<wsrep::mutex>& lock,
                     wsrep::seqno bf_seqno)
        {
            return transaction_.bf_abort(lock, bf_seqno);
        }
        /*!
         * Get reference to the client mutex.
         *
         * \return Reference to the client mutex.
         */
        wsrep::mutex& mutex() { return mutex_; }

        /*!
         * Get server context associated the the client session.
         *
         * \return Reference to server context.
         */
        wsrep::server_context& server_context() const
        { return server_context_; }

        /*!
         * Get reference to the Provider which is associated
         * with the client context.
         *
         * \return Reference to the provider.
         * \throw wsrep::runtime_error if no providers are associated
         *        with the client context.
         */
        wsrep::provider& provider() const;

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

        const wsrep::transaction_context& transaction() const
        {
            return transaction_;
        }

        void debug_log_level(int level) { debug_log_level_ = level; }
        int debug_log_level() const
        {
            return std::max(debug_log_level_,
                            server_context_.debug_log_level());
        }

        void reset_error()
        {
            current_error_ = wsrep::e_success;
        }

        enum wsrep::client_error current_error() const
        {
            return current_error_;
        }
    protected:
        /*!
         * Client context constuctor. This is protected so that it
         * can be called from derived class constructors only.
         */
        client_context(wsrep::mutex& mutex,
                       wsrep::server_context& server_context,
                       const client_id& id,
                       enum mode mode)
            : thread_id_(wsrep::this_thread::get_id())
            , mutex_(mutex)
            , server_context_(server_context)
            , id_(id)
            , mode_(mode)
            , state_(s_idle)
            , transaction_(*this)
            , allow_dirty_reads_()
            , debug_log_level_(0)
            , current_error_(wsrep::e_success)
        { }

    private:
        client_context(const client_context&);
        client_context& operator=(client_context&);

        /*
         * Friend declarations
         */
        friend int server_context::on_apply(client_context&,
                                            const wsrep::ws_handle&,
                                            const wsrep::ws_meta&,
                                            const wsrep::data&);
        friend class client_context_switch;
        friend class client_applier_mode;
        friend class client_toi_mode;
        friend class transaction_context;


        /*!
         * Set client state.
         */
        void state(wsrep::unique_lock<wsrep::mutex>& lock, enum state state);

        virtual bool is_autocommit() const = 0;
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
        virtual int append_fragment(wsrep::transaction_context&,
                                    int, const wsrep::data&)
        { return 0; }


        /*!
         * This method applies a write set give in data buffer.
         * This must be implemented by the DBMS integration.
         *
         * \return Zero on success, non-zero on applying failure.
         */
        virtual int apply(const wsrep::data& data) = 0;

        /*!
         * Virtual method which will be called
         * in order to commit the transaction into
         * storage engine.
         *
         * \return Zero on success, non-zero on failure.
         */
        virtual int commit() = 0;

        /*!
         * Rollback the transaction.
         *
         * This metod must be implemented by DBMS integration.
         *
         * \return Zero on success, no-zero on failure.
         */
        virtual int rollback() = 0;

        /*!
         * Notify a implementation that the client is about
         * to replay the transaction.
         */
        virtual void will_replay(wsrep::transaction_context&) = 0;

        /*!
         * Replay the transaction.
         */
        virtual int replay(wsrep::transaction_context& tc) = 0;


        /*!
         * Wait until all of the replaying transactions have been committed.
         */
        virtual void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>&) = 0;

        virtual int prepare_data_for_replication(
            const wsrep::transaction_context&, wsrep::data& data) = 0;

        /*!
         * Return true if the current client operation was killed.
         */
        virtual bool killed() const = 0;

        /*!
         * Abort server operation on fatal error. This should be used
         * only for critical conditions which would sacrifice data
         * consistency.
         */
        virtual void abort() = 0;

    public:
        /*!
         * Set up thread global variables for client connection.
         */
        virtual void store_globals()
        {
            thread_id_ = wsrep::this_thread::get_id();
        }
    private:

        /*!
         * Enter debug synchronization point.
         */
        virtual void debug_sync(wsrep::unique_lock<wsrep::mutex>&, const char*) = 0;

        /*!
         *
         */
        virtual void debug_suicide(const char*) = 0;

        /*!
         * Notify the implementation about an error.
         */
        virtual void on_error(enum wsrep::client_error error) = 0;
        /*!
         *
         */
        void override_error(enum wsrep::client_error error);

        wsrep::thread::id thread_id_;
        wsrep::mutex& mutex_;
        wsrep::server_context& server_context_;
        client_id id_;
        enum mode mode_;
        enum state state_;
    protected:
        wsrep::transaction_context transaction_;
    private:
        /*!
         * \todo This boolean should be converted to better read isolation
         * semantics.
         */
        bool allow_dirty_reads_;
        int debug_log_level_;
        wsrep::client_error current_error_;
    };


    class client_context_switch
    {
    public:
        client_context_switch(wsrep::client_context& orig_context,
                              wsrep::client_context& current_context)
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

    class client_applier_mode
    {
    public:
        client_applier_mode(wsrep::client_context& client)
            : client_(client)
            , orig_mode_(client.mode_)
        {
            client_.mode_ = wsrep::client_context::m_applier;
        }
        ~client_applier_mode()
        {
            client_.mode_ = orig_mode_;
        }
    private:
        wsrep::client_context& client_;
        enum wsrep::client_context::mode orig_mode_;
    };

    class client_toi_mode
    {
    public:
        client_toi_mode(wsrep::client_context& client)
            : client_(client)
            , orig_mode_(client.mode_)
        {
            client_.mode_ = wsrep::client_context::m_toi;
        }
        ~client_toi_mode()
        {
            assert(client_.mode() == wsrep::client_context::m_toi);
            client_.mode_ = orig_mode_;
        }
    private:
        wsrep::client_context& client_;
        enum wsrep::client_context::mode orig_mode_;
    };


}

#endif // WSREP_CLIENT_CONTEXT_HPP
