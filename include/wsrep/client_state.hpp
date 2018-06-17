//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/** @file client_state.hpp
 *
 */

#ifndef WSREP_CLIENT_CONTEXT_HPP
#define WSREP_CLIENT_CONTEXT_HPP

#include "server_state.hpp"
#include "provider.hpp"
#include "transaction.hpp"
#include "client_id.hpp"
#include "client_service.hpp"
#include "mutex.hpp"
#include "lock.hpp"
#include "buffer.hpp"
#include "thread.hpp"
#include "logger.hpp"

namespace wsrep
{
    class server_state;
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

    /**
     * Client State
     */
    class client_state
    {
    public:
        /**
         * Client mode enumeration.
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

        /**
         * Store variables related to global execution context.
         * This method should be called every time the thread
         * operating the client state changes.
         */
        void store_globals()
        {
            thread_id_ = wsrep::this_thread::get_id();
        }

        /**
         * Destructor.
         */
        virtual ~client_state()
        {
            assert(transaction_.active() == false);
        }

        /**
         * Return true if the transaction commit requires
         * two-phase commit.
         */
        bool do_2pc() const
        {
            return client_service_.do_2pc();
        }

        /**
         * This mehod should be called before the processing of command
         * received from DBMS client starts.
         *
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
         * This method should be called before returning
         * a result to DBMS client.
         *
         * The method will check if the transaction associated to
         * the connection has been aborted. Rollback is performed
         * if needed. After the call, current_error() will return an error
         * code associated to the client state. If the error code is
         * not success, the transaction associated to the client state
         * has been aborted and rolled back.
         */
        void after_command_before_result();

        /**
         * Method which should be called after returning the
         * control back to DBMS client..
         *
         * The method will do the check if the transaction associated
         * to the connection has been aborted. If so, rollback is
         * performed and the transaction is left to aborted state.
         * The next call to before_command() will return an error and
         * the error state can be examined after after_command_before_resul()
         * is called.
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
         */
        enum after_statement_result after_statement();

        //
        // Replicating interface
        //
        /**
         * Start a new transaction with a transaction id.
         *
         * @todo This method should
         *       - Register the transaction on server level for bookkeeping
         *       - Isolation levels? Or part of the transaction?
         */
        int start_transaction(const wsrep::transaction_id& id)
        {
            assert(state_ == s_exec);
            return transaction_.start_transaction(id);
        }

        /**
         * Append a key into transaction write set.
         */
        int append_key(const wsrep::key& key)
        {
            assert(mode_ == m_replicating);
            assert(state_ == s_exec);
            return transaction_.append_key(key);
        }

        /**
         * Append data into transaction write set.
         */
        int append_data(const wsrep::const_buffer& data)
        {
            assert(mode_ == m_replicating);
            assert(state_ == s_exec);
            return transaction_.append_data(data);
        }

        int prepare_data_for_replication(const wsrep::transaction& tc)
        {
            return client_service_.prepare_data_for_replication(*this, tc);
        }

        //
        // Streaming interface
        //
        /**
         * This method should be called after every row operation.
         */
        int after_row()
        {
            assert(mode_ == m_replicating);
            assert(state_ == s_exec);
            return (transaction_.streaming_context_.fragment_size() ?
                    transaction_.after_row() : 0);
        }

        /**
         * Enable streaming replication.
         *
         * Currently it is not possible to change the fragment unit
         * for active streaming transaction.
         *
         * @param fragment_unit Desired fragment unit
         * @param fragment_size Desired fragment size
         *
         * @return Zero on success, non-zero if the streaming cannot be
         *         enabled.
         */
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

        /** @todo deprecate */
        size_t bytes_generated() const
        {
            assert(mode_ == m_replicating);
            return client_service_.bytes_generated();
        }

        /** @todo deprecate */
        int prepare_fragment_for_replication(
            const wsrep::transaction& tc,
            wsrep::mutable_buffer& mb)
        {
            return client_service_.prepare_fragment_for_replication(
                *this, tc, mb);
        }

        /** @todo deprecate */
        int append_fragment(const wsrep::transaction& tc,
                            int flags,
                            const wsrep::const_buffer& buf)
        {
            return client_service_.append_fragment(tc, flags, buf);
        }
        /**
         * Remove fragments from the fragment storage. If the
         * storage is transactional, this should be done within
         * the same transaction which is committing.
         *
         * @todo deprecate
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
        int bf_abort(wsrep::seqno bf_seqno)
        {
            wsrep::unique_lock<wsrep::mutex> lock(mutex_);
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

        void adopt_transaction(wsrep::transaction& transaction)
        {
            assert(mode_ == m_high_priority);
            transaction_.start_transaction(transaction.id());
            transaction_.streaming_context_ = transaction.streaming_context_;
        }

        enum wsrep::provider::status replay(
            wsrep::transaction& tc)
        {
            return client_service_.replay(*this, tc);
        }

        //
        //
        //
        void will_replay(const wsrep::transaction& tc)
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
        // Causal reads
        //

        /**
         * Perform a causal read in the cluster. After the call returns,
         * all the causally preceding write sets have been committed
         * or the error is returned.
         *
         * This operation may require communication with other processes
         * in the DBMS cluster, so it may be relatively heavy operation.
         * Method wait_for_gtid() should be used whenever possible.
         */
        int causal_read() const;

        /**
         * Wait until all the write sets up to given GTID have been
         * committed.
         *
         * @return Zero on success, non-zero on failure.
         */
        int wait_for_gtid(const wsrep::gtid&) const;

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
        wsrep::server_state& server_state() const
        { return server_state_; }

        /**
         * Get reference to the Provider which is associated
         * with the client context.
         *
         * @return Reference to the provider.
         * @throw wsrep::runtime_error if no providers are associated
         *        with the client context.
         *
         * @todo Should be removed.
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

        /**
         * Return a const reference to the transaction associated
         * with the client state.
         */
        const wsrep::transaction& transaction() const
        {
            return transaction_;
        }

        /**
         * Set debug logging level.
         *
         * Levels:
         * 0 - Debug logging is disabled
         * 1..n - Debug logging with increasing verbosity.
         */
        void debug_log_level(int level) { debug_log_level_ = level; }

        /**
         * Return current debug logging level. The return value
         * is a maximum of client state and server state debug log
         * levels.
         *
         * @return Current debug log level.
         */
        int debug_log_level() const
        {
            return std::max(debug_log_level_,
                            server_state_.debug_log_level());
        }

        //
        // Error handling
        //

        /**
         * Reset the current error state.
         *
         * @todo There should be some protection about when this can
         *       be done.
         */
        void reset_error()
        {
            current_error_ = wsrep::e_success;
        }

        /**
         * Return current error code.
         *
         * @return Current error code.
         */
        enum wsrep::client_error current_error() const
        {
            return current_error_;
        }

    protected:
        /**
         * Client context constuctor. This is protected so that it
         * can be called from derived class constructors only.
         */
        client_state(wsrep::mutex& mutex,
                     wsrep::server_state& server_state,
                     wsrep::client_service& client_service,
                     const client_id& id,
                     enum mode mode)
            : thread_id_(wsrep::this_thread::get_id())
            , mutex_(mutex)
            , server_state_(server_state)
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
        client_state(const client_state&);
        client_state& operator=(client_state&);

        friend class client_state_switch;
        friend class high_priority_context;
        friend class client_toi_mode;
        friend class transaction;

        void debug_log_state(const char*) const;
        void state(wsrep::unique_lock<wsrep::mutex>& lock, enum state state);
        void override_error(enum wsrep::client_error error);

        wsrep::thread::id thread_id_;
        wsrep::mutex& mutex_;
        wsrep::server_state& server_state_;
        wsrep::client_service& client_service_;
        wsrep::client_id id_;
        enum mode mode_;
        enum state state_;
        wsrep::transaction transaction_;
        bool allow_dirty_reads_;
        int debug_log_level_;
        wsrep::client_error current_error_;
    };

    class client_state_switch
    {
    public:
        client_state_switch(wsrep::client_state& orig_context,
                              wsrep::client_state& current_context)
            : orig_context_(orig_context)
            , current_context_(current_context)
        {
            current_context_.client_service_.store_globals();
        }
        ~client_state_switch()
        {
            orig_context_.client_service_.store_globals();
        }
    private:
        client_state& orig_context_;
        client_state& current_context_;
    };


    /**
     * Utility class to switch the client state to high priority
     * mode. The client is switched back to the original mode
     * when the high priority context goes out of scope.
     */
    class high_priority_context
    {
    public:
        high_priority_context(wsrep::client_state& client)
            : client_(client)
            , orig_mode_(client.mode_)
        {
            client_.mode_ = wsrep::client_state::m_high_priority;
        }
        virtual ~high_priority_context()
        {
            client_.mode_ = orig_mode_;
        }
    private:
        wsrep::client_state& client_;
        enum wsrep::client_state::mode orig_mode_;
    };

    /**
     *
     */
    class client_toi_mode
    {
    public:
        client_toi_mode(wsrep::client_state& client)
            : client_(client)
            , orig_mode_(client.mode_)
        {
            client_.mode_ = wsrep::client_state::m_toi;
        }
        ~client_toi_mode()
        {
            assert(client_.mode() == wsrep::client_state::m_toi);
            client_.mode_ = orig_mode_;
        }
    private:
        wsrep::client_state& client_;
        enum wsrep::client_state::mode orig_mode_;
    };

    template <class D>
    class scoped_client_state
    {
    public:
        scoped_client_state(wsrep::client_state* client_state, D deleter)
            : client_state_(client_state)
            , deleter_(deleter)
        {
            if (client_state_ == 0)
            {
                throw wsrep::runtime_error("Null client_state provided");
            }
        }
        wsrep::client_state& client_state() { return *client_state_; }
        ~scoped_client_state()
        {
            deleter_(client_state_);
        }
    private:
        scoped_client_state(const scoped_client_state&);
        scoped_client_state& operator=(const scoped_client_state&);
        wsrep::client_state* client_state_;
        D deleter_;
    };
}

#endif // WSREP_CLIENT_CONTEXT_HPP
