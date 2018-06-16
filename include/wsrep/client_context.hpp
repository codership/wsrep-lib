//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/** @file client_context.hpp
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
#include "provider.hpp"
#include "transaction_context.hpp"
#include "client_id.hpp"
#include "client_service.hpp"
#include "mutex.hpp"
#include "lock.hpp"
#include "buffer.hpp"
#include "thread.hpp"
#include "logger.hpp"

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

    /** @class Client Context
     *
     * Client Contex abstract interface.
     */
    class client_context
    {
    public:
        /**
         * Client mode enumeration.
         * @todo m_toi total order isolation mode
         */
        enum mode
        {
            /** Operates in local only mode, no replication. */
            m_local,
            /** Generates write sets for replication by the provider. */
            m_replicating,
            /** High priority mode */
            m_high_priority,
            /** Client is in total order isolation mode */
            m_toi
        };

        /**
         * Client state enumeration.
         *
         */
        enum state
        {
            /**
             * Client is idle, the control is in the application which
             * uses the DBMS system.
             */
            s_idle,
            /**
             * The control of the client processing is inside the DBMS
             * system.
             */
            s_exec,
            /**
             * Client handler is sending result to client.
             */
            s_result,
            /**
             * The client session is terminating.
             */
            s_quitting
        };

        const static int state_max_ = s_quitting + 1;


        void store_globals()
        {
            thread_id_ = wsrep::this_thread::get_id();
        }

        /**
         * Destructor.
         */
        virtual ~client_context()
        {
            assert(transaction_.active() == false);
        }

        /**
         *
         */
        bool do_2pc() const
        {
            return client_service_.do_2pc();
        }

        /**
         * Method which should be called before the client
         * starts processing the command received from the application.
         * This method will wait until the possible synchronous
         * rollback for associated transaction has finished.
         * The method has a side effect of changing the client
         * context state to executing.
         *
         * @return Zero in case of success, non-zero in case of the
         *         associated transaction was BF aborted.
         */
        int before_command();

        /**
         * Method which should be called before returning
         * the control back to application which uses the DBMS system.
         * This method will check if the transaction associated to
         * the connection has been aborted. Rollback is performed
         * if needed.
         */
        void after_command_before_result();

        /**
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

        /**
         * Before statement execution operations.
         *
         * Check if server is synced and if dirty reads are allowed.
         *
         * If the method is overridden by the implementation, base class
         * method should be called before any implementation specifc
         * operations.
         *
         * @return Zero in case of success, non-zero if the statement
         *         is not allowed to be executed due to read or write
         *         isolation requirements.
         */
        int before_statement();

        /**
         * Return values for after_statement() method.
         */
        enum after_statement_result
        {
            /** Statement was executed succesfully */
            asr_success,
            /** Statement execution encountered an error, the transaction
             * was rolled back */
            asr_error,
            /** Statement execution encountered an error, the transaction
              was rolled back. However the statement was self contained
              (e.g. autocommit statement) so it can be retried. */
            asr_may_retry
        };
        /**
         * After statement execution operations.
         *
         * * Check for must_replay state
         * * Do rollback if requested
         *
         * If overridden by the implementation, base class method
         * should be called after any implementation specific operations.
         */
        enum after_statement_result after_statement();

        //
        // Replicating interface
        //
        int start_transaction(const wsrep::transaction_id& id)
        {
            assert(state_ == s_exec);
            return transaction_.start_transaction(id);
        }

        int append_key(const wsrep::key& key)
        {
            assert(mode_ == m_replicating);
            assert(state_ == s_exec);
            return transaction_.append_key(key);
        }

        int append_data(const wsrep::const_buffer& data)
        {
            assert(mode_ == m_replicating);
            assert(state_ == s_exec);
            return transaction_.append_data(data);
        }

        int prepare_data_for_replication(const wsrep::transaction_context& tc)
        {
            return client_service_.prepare_data_for_replication(*this, tc);
        }

        //
        // Streaming interface
        //
        int after_row()
        {
            assert(mode_ == m_replicating);
            assert(state_ == s_exec);
            return transaction_.after_row();
        }

        int enable_streaming(
            enum wsrep::streaming_context::fragment_unit
            fragment_unit,
            size_t fragment_size)
        {
            assert(mode_ == m_replicating);
            if (transaction_.active() &&
                transaction_.streaming_context_.fragment_unit() !=
                fragment_unit)
            {
                wsrep::log_error()
                    << "Changing fragment unit for active transaction "
                    << "not allowed";
                return 1;
            }
            transaction_.streaming_context_.enable(
                fragment_unit, fragment_size);
            return 0;
        }

        size_t bytes_generated() const
        {
            assert(mode_ == m_replicating);
            return client_service_.bytes_generated();
        }

        int prepare_fragment_for_replication(
            const wsrep::transaction_context& tc,
            wsrep::mutable_buffer& mb)
        {
            return client_service_.prepare_fragment_for_replication(
                *this, tc, mb);
        }

        int append_fragment(const wsrep::transaction_context& tc,
                            int flags,
                            const wsrep::const_buffer& buf)
        {
            return client_service_.append_fragment(tc, flags, buf);
        }
        /**
         * Remove fragments from the fragment storage. If the
         * storage is transactional, this should be done within
         * the same transaction which is committing.
         */
        void remove_fragments()
        {
            client_service_.remove_fragments(transaction_);
        }

        //
        // Applying interface
        //

        int start_transaction(const wsrep::ws_handle& wsh,
                              const wsrep::ws_meta& meta)
        {
            assert(mode_ == m_high_priority);
            return transaction_.start_transaction(wsh, meta);
        }

        int apply(const wsrep::const_buffer& data)
        {
            assert(mode_ == m_high_priority);
            return client_service_.apply(*this, data);
        }

        int commit()
        {
            assert(mode_ == m_high_priority || mode_ == m_local);
            return client_service_.commit(
                *this,
                transaction_.ws_handle(), transaction_.ws_meta());
        }
        //
        // Commit ordering
        //

        int before_prepare()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(state_ == s_exec);
            return transaction_.before_prepare(lock);
        }

        int after_prepare()
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
            assert(state_ == s_exec);
            return transaction_.after_prepare(lock);
        }

        int before_commit()
        {
            assert(state_ == s_exec || mode_ == m_local);
            return transaction_.before_commit();
        }

        int ordered_commit()
        {
            assert(state_ == s_exec || mode_ == m_local);
            return transaction_.ordered_commit();
        }

        int after_commit()
        {
            assert(state_ == s_exec || mode_ == m_local);
            return transaction_.after_commit();
        }

        //
        // Rollback
        //
        int rollback()
        {
            return client_service_.rollback(*this);
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

        //
        // BF aborting
        //
        int bf_abort(wsrep::unique_lock<wsrep::mutex>& lock,
                     wsrep::seqno bf_seqno)
        {
            assert(mode_ == m_replicating);
            return transaction_.bf_abort(lock, bf_seqno);
        }

        //
        // Replaying
        //


        int start_replaying(const wsrep::ws_meta& ws_meta)
        {
            assert(mode_ == m_high_priority);
            return transaction_.start_replaying(ws_meta);
        }

        void adopt_transaction(wsrep::transaction_context& transaction)
        {
            assert(mode_ == m_high_priority);
            transaction_.start_transaction(transaction.id());
            transaction_.streaming_context_ = transaction.streaming_context_;
        }

        enum wsrep::provider::status replay(
            wsrep::transaction_context& tc)
        {
            return client_service_.replay(*this, tc);
        }

        //
        //
        //

        void will_replay(const wsrep::transaction_context& tc)
        {
            client_service_.will_replay(tc);
        }
        void wait_for_replayers(wsrep::unique_lock<wsrep::mutex>& lock)
        {
            client_service_.wait_for_replayers(*this, lock);
        }

        bool interrupted() const
        {
            return client_service_.interrupted();
        }

        void emergency_shutdown()
        {
            client_service_.emergency_shutdown();
        }

        //
        // Debug interface
        //
        void debug_sync(const char* sync_point)
        {
            client_service_.debug_sync(*this, sync_point);
        }

        void debug_crash(const char* crash_point)
        {
            client_service_.debug_crash(crash_point);
        }
        /**
         * Get reference to the client mutex.
         *
         * @return Reference to the client mutex.
         */
        wsrep::mutex& mutex() { return mutex_; }

        /**
         * Get server context associated the the client session.
         *
         * @return Reference to server context.
         */
        wsrep::server_context& server_context() const
        { return server_context_; }

        /**
         * Get reference to the Provider which is associated
         * with the client context.
         *
         * @return Reference to the provider.
         * @throw wsrep::runtime_error if no providers are associated
         *        with the client context.
         */
        wsrep::provider& provider() const;

        /**
         * Get Client identifier.
         *
         * @return Client Identifier
         */
        client_id id() const { return id_; }

        /**
         * Get Client mode.
         *
         * @todo Enforce mutex protection if called from other threads.
         *
         * @return Client mode.
         */
        enum mode mode() const { return mode_; }
        /**
         * Get Client state.
         *
         * @todo Enforce mutex protection if called from other threads.
         *
         * @return Client state
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
        /**
         * Client context constuctor. This is protected so that it
         * can be called from derived class constructors only.
         */
        client_context(wsrep::mutex& mutex,
                       wsrep::server_context& server_context,
                       wsrep::client_service& client_service,
                       const client_id& id,
                       enum mode mode)
            : thread_id_(wsrep::this_thread::get_id())
            , mutex_(mutex)
            , server_context_(server_context)
            , client_service_(client_service)
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

        friend class client_context_switch;
        friend class high_priority_context;
        friend class client_toi_mode;
        friend class transaction_context;

        void debug_log_state(const char*) const;
        /**
         * Set client state.
         */
        void state(wsrep::unique_lock<wsrep::mutex>& lock, enum state state);

        void override_error(enum wsrep::client_error error);

        wsrep::thread::id thread_id_;
        wsrep::mutex& mutex_;
        wsrep::server_context& server_context_;
        wsrep::client_service& client_service_;
        client_id id_;
        enum mode mode_;
        enum state state_;
    protected:
        wsrep::transaction_context transaction_;
    private:
        /**
         * @todo This boolean should be converted to better read isolation
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
            current_context_.client_service_.store_globals();
        }
        ~client_context_switch()
        {
            orig_context_.client_service_.store_globals();
        }
    private:
        client_context& orig_context_;
        client_context& current_context_;
    };

    class high_priority_context
    {
    public:
        high_priority_context(wsrep::client_context& client)
            : client_(client)
            , orig_mode_(client.mode_)
        {
            client_.mode_ = wsrep::client_context::m_high_priority;
        }
        ~high_priority_context()
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

    template <class D>
    class scoped_client_context
    {
    public:
        scoped_client_context(wsrep::client_context* client_context, D deleter)
            : client_context_(client_context)
            , deleter_(deleter)
        {
            if (client_context_ == 0)
            {
                throw wsrep::runtime_error("Null client_context provided");
            }
        }
        wsrep::client_context& client_context() { return *client_context_; }
        ~scoped_client_context()
        {
            deleter_(client_context_);
        }
    private:
        scoped_client_context(const scoped_client_context&);
        scoped_client_context& operator=(const scoped_client_context&);
        wsrep::client_context* client_context_;
        D deleter_;
    };
}

#endif // WSREP_CLIENT_CONTEXT_HPP
