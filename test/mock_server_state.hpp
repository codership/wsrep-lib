//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_SERVER_CONTEXT_HPP
#define WSREP_MOCK_SERVER_CONTEXT_HPP

#include "wsrep/server_state.hpp"
#include "wsrep/server_service.hpp"
#include "mock_client_state.hpp"
#include "mock_high_priority_service.hpp"
#include "mock_storage_service.hpp"
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
            , last_transaction_id_(0)
        { }

        wsrep::mock_provider& provider() const
        { return provider_; }

        wsrep::storage_service* storage_service(wsrep::client_service&)
        {
            return new wsrep::mock_storage_service(*this,
                                                   wsrep::client_id(++last_client_id_));
        }

        wsrep::storage_service* storage_service(wsrep::high_priority_service&)
        {
            return new wsrep::mock_storage_service(*this,
                                                   wsrep::client_id(++last_client_id_));
        }

        void release_storage_service(wsrep::storage_service* storage_service)
        {
            delete storage_service;
        }

        wsrep::client_state* local_client_state()
        {
            wsrep::client_state* ret(new wsrep::mock_client(
                                         *this,
                                         wsrep::client_id(++last_client_id_),
                                         wsrep::client_state::m_local));
            ret->open(ret->id());
            return ret;
        }

        void release_client_state(wsrep::client_state* client_state)
        {
            delete client_state;
        }

        wsrep::high_priority_service* streaming_applier_service(
            wsrep::client_service&)
        {
            wsrep::mock_client* cs(new wsrep::mock_client(
                                       *this,
                                       wsrep::client_id(++last_client_id_),
                                       wsrep::client_state::m_high_priority));
            wsrep::mock_high_priority_service* ret(
                new wsrep::mock_high_priority_service(*this, cs, false));
            cs->open(cs->id());
            cs->before_command();
            return ret;
        }

        wsrep::high_priority_service* streaming_applier_service(
            wsrep::high_priority_service&)
        {
            wsrep::mock_client* cs(new wsrep::mock_client(
                                       *this,
                                       wsrep::client_id(++last_client_id_),
                                       wsrep::client_state::m_high_priority));
            wsrep::mock_high_priority_service* ret(
                new wsrep::mock_high_priority_service(*this, cs, false));
            cs->open(cs->id());
            cs->before_command();
            return ret;
        }

        void release_high_priority_service(
            wsrep::high_priority_service *high_priority_service)
            WSREP_OVERRIDE
        {
            mock_high_priority_service* mhps(
                reinterpret_cast<mock_high_priority_service*>(high_priority_service));
            wsrep::client_state* cs(mhps->client_state());
            cs->after_command_before_result();
            cs->after_command_after_result();
            cs->close();
            cs->cleanup();
            delete cs;
            delete mhps;
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
        void log_view(wsrep::high_priority_service*, const wsrep::view&) { }
        void log_state_change(enum wsrep::server_state::state,
                              enum wsrep::server_state::state)
        { }
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

        int wait_committing_transactions(int) WSREP_OVERRIDE { return 0; }

        wsrep::transaction_id next_transaction_id()
        {
            return wsrep::transaction_id(++last_transaction_id_);
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
        unsigned long long last_transaction_id_;
    };
}

#endif // WSREP_MOCK_SERVER_CONTEXT_HPP
