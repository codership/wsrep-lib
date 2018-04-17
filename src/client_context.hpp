//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_CLIENT_CONTEXT_HPP
#define TRREP_CLIENT_CONTEXT_HPP

#include "provider.hpp"
#include "mutex.hpp"
#include "lock.hpp"
#include "data.hpp"

namespace trrep
{
    class server_context;
    class transaction_context;

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

    class client_context
    {
    public:
        enum mode
        {
            m_local,       // Operates in local only mode, no replication
            m_replicating, // Generates write sets for replication
            m_applier      // Applying write sets from provider
        };

        enum state
        {
            s_idle,
            s_exec,
            s_quitting
        };

        client_context(trrep::server_context& server_context,
                       client_id id,
                       enum mode mode)
            : mutex_()
            , server_context_(server_context)
            , id_(id)
            , mode_(mode)
            , state_(s_idle)
        { }

        virtual ~client_context() { }
        // Accessors
        trrep::mutex& mutex() { return mutex_; }
        trrep::server_context& server_context() const
        { return server_context_; }

        client_id id() const { return id_; }
        client_id id(const client_id& id)
        {
            assert(mode() == m_applier);
            id_ = id;
        }

        enum mode mode() const { return mode_; }
        enum state state() const { return state_; }

        //
        //
        //
        virtual void before_command() { }

        virtual void after_command() { }

        virtual int before_statement() { return 0; }

        virtual int after_statement() { return 0; }

        virtual int append_fragment(trrep::transaction_context&,
                                    uint32_t, const trrep::data&)
        { return 0; }
        virtual int commit(trrep::transaction_context&) { return 0; }
        virtual int rollback(trrep::transaction_context&) { return 0; }

        virtual void will_replay(trrep::transaction_context&) { }
        virtual int replay(trrep::transaction_context& tc);


        virtual int apply(const trrep::data&) { return 0; }

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
    private:

        void state(enum state state);

        trrep::mutex mutex_;
        trrep::server_context& server_context_;
        client_id id_;
        enum mode mode_;
        enum state state_;
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
