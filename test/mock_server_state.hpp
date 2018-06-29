//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_SERVER_CONTEXT_HPP
#define WSREP_MOCK_SERVER_CONTEXT_HPP

#include "wsrep/server_state.hpp"
#include "mock_client_state.hpp"
#include "mock_provider.hpp"

#include "wsrep/compiler.hpp"

namespace wsrep
{
    class mock_server_state
        : public wsrep::server_state
        , public wsrep::server_service
    {
    public:
        mock_server_state(const std::string& name,
                            const std::string& id,
                            enum wsrep::server_state::rollback_mode rollback_mode)
            : wsrep::server_state(mutex_, cond_, *this,
                                  name, id, "", "", "./", wsrep::gtid::undefined(), 1, rollback_mode)
            , sync_point_enabled_()
            , sync_point_action_()
            , sst_before_init_()
            , mutex_()
            , cond_()
            , provider_(*this)
            , last_client_id_(0)
        { }
        wsrep::mock_provider& provider() const
        { return provider_; }
        wsrep::client_state* local_client_state()
        {
            wsrep::client_state* ret(new wsrep::mock_client(
                                         *this, ++last_client_id_,
                                         wsrep::client_state::m_local));
            ret->open(ret->id());
            return ret;
        }
        wsrep::client_state* streaming_applier_client_state()
        {
            wsrep::client_state* ret(
                new wsrep::mock_client(
                    *this, ++last_client_id_,
                    wsrep::client_state::m_high_priority));
            ret->open(ret->id());
            return ret;
        }

        void release_client_state(wsrep::client_state* client_state)
        {
            delete client_state;
        }
        void bootstrap() WSREP_OVERRIDE { }
        void log_message(enum wsrep::log::level level, const char* message)
        {
            wsrep::log(level, name().c_str()) << message;
        }
        void log_dummy_write_set(wsrep::client_state&,
                                 const wsrep::ws_meta&)
            WSREP_OVERRIDE
        {
        }
        void log_view(const wsrep::view&) { }

        // void on_connect(const wsrep::gtid& ) WSREP_OVERRIDE { }
        // void wait_until_connected() WSREP_OVERRIDE { }
        // void on_view(const wsrep::view&) WSREP_OVERRIDE { }
        // void on_sync() WSREP_OVERRIDE { }
        bool sst_before_init() const WSREP_OVERRIDE
        { return sst_before_init_; }
        std::string sst_request() WSREP_OVERRIDE { return ""; }
        int start_sst(const std::string&,
                      const wsrep::gtid&,
                      bool) WSREP_OVERRIDE { return 0; }
        void background_rollback(wsrep::client_state& client_state)
            WSREP_OVERRIDE
        {
            client_state.before_rollback();
            client_state.after_rollback();
        }

        void debug_sync(const char* sync_point) WSREP_OVERRIDE
        {
            if (sync_point_enabled_ == sync_point)
            {
                switch (sync_point_action_)
                {
                case spa_initialize:
                    initialized();
                    break;
                }
            }
        }

        std::string sync_point_enabled_;
        enum sync_point_action
        {
            spa_initialize
        } sync_point_action_;
        bool sst_before_init_;

    private:
        wsrep::default_mutex mutex_;
        wsrep::default_condition_variable cond_;
        mutable wsrep::mock_provider provider_;
        unsigned long long last_client_id_;
    };
}

#endif // WSREP_MOCK_SERVER_CONTEXT_HPP
