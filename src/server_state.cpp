//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/server_state.hpp"
#include "wsrep/client_state.hpp"
#include "wsrep/transaction.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/id.hpp"

#include <cassert>
#include <sstream>

namespace
{
    int apply_write_set(wsrep::server_state& server_state,
                        wsrep::client_state& client_state,
                        const wsrep::ws_handle& ws_handle,
                        const wsrep::ws_meta& ws_meta,
                        const wsrep::const_buffer& data)
    {
        int ret(0);
        const wsrep::transaction& txc(client_state.transaction());
        assert(client_state.mode() == wsrep::client_state::m_high_priority);

        bool not_replaying(txc.state() !=
                           wsrep::transaction::s_replaying);

        if (wsrep::starts_transaction(ws_meta.flags()) &&
            wsrep::commits_transaction(ws_meta.flags()))
        {
            if (not_replaying)
            {
                client_state.before_command();
                client_state.before_statement();
                assert(txc.active() == false);
                client_state.start_transaction(ws_handle, ws_meta);
            }
            else
            {
                client_state.start_replaying(ws_meta);
            }

            if (client_state.client_service().apply_write_set(data))
            {
                ret = 1;
            }
            else if (client_state.client_service().commit(ws_handle, ws_meta))
            {
                ret = 1;
            }

            if (ret)
            {
                client_state.client_service().rollback();
            }

            if (not_replaying)
            {
                client_state.after_statement();
                client_state.after_command_before_result();
                client_state.after_command_after_result();
            }
            assert(ret ||
                   txc.state() == wsrep::transaction::s_committed);
        }
        else if (wsrep::starts_transaction(ws_meta.flags()))
        {
            assert(not_replaying);
            assert(server_state.find_streaming_applier(
                       ws_meta.server_id(), ws_meta.transaction_id()) == 0);
            wsrep::client_state* sac(
                server_state.server_service().streaming_applier_client_state());
            server_state.start_streaming_applier(
                ws_meta.server_id(), ws_meta.transaction_id(), sac);
            sac->start_transaction(ws_handle, ws_meta);
            {
                // TODO: Client context switch will be ultimately
                // implemented by the application. Therefore need to
                // introduce a virtual method call which takes
                // both original and sac client contexts as argument
                // and a functor which does the applying.
                wsrep::client_state_switch sw(client_state, *sac);
                sac->before_command();
                sac->before_statement();
                sac->client_service().apply_write_set(data);
                sac->after_statement();
                sac->after_command_before_result();
                sac->after_command_after_result();
            }
            server_state.server_service().log_dummy_write_set(client_state, ws_meta);
        }
        else if (ws_meta.flags() == 0)
        {
            wsrep::client_state* sac(
                server_state.find_streaming_applier(
                    ws_meta.server_id(), ws_meta.transaction_id()));
            if (sac == 0)
            {
                // It is possible that rapid group membership changes
                // may cause streaming transaction be rolled back before
                // commit fragment comes in. Although this is a valid
                // situation, log a warning if a sac cannot be found as
                // it may be an indication of  a bug too.
                wsrep::log_warning() << "Could not find applier context for "
                                     << ws_meta.server_id()
                                     << ": " << ws_meta.transaction_id();
            }
            else
            {
                wsrep::client_state_switch(client_state, *sac);
                sac->before_command();
                sac->before_statement();
                ret = sac->client_service().apply_write_set(data);
                sac->after_statement();
                sac->after_command_before_result();
                sac->after_command_after_result();
            }
            server_state.server_service().log_dummy_write_set(
                client_state, ws_meta);
        }
        else if (wsrep::commits_transaction(ws_meta.flags()))
        {
            if (not_replaying)
            {
                wsrep::client_state* sac(
                    server_state.find_streaming_applier(
                        ws_meta.server_id(), ws_meta.transaction_id()));
                assert(sac);
                if (sac == 0)
                {
                    // It is possible that rapid group membership changes
                    // may cause streaming transaction be rolled back before
                    // commit fragment comes in. Although this is a valid
                    // situation, log a warning if a sac cannot be found as
                    // it may be an indication of  a bug too.
                    wsrep::log_warning()
                        << "Could not find applier context for "
                        << ws_meta.server_id()
                        << ": " << ws_meta.transaction_id();
                }
                else
                {
                    wsrep::client_state_switch(client_state, *sac);
                    sac->before_command();
                    sac->before_statement();
                    ret = sac->client_service().commit(ws_handle, ws_meta);
                    sac->after_statement();
                    sac->after_command_before_result();
                    sac->after_command_after_result();
                    server_state.stop_streaming_applier(
                        ws_meta.server_id(), ws_meta.transaction_id());
                    server_state.server_service().release_client_state(sac);
                }
            }
            else
            {
                ret = client_state.start_replaying(ws_meta) ||
                    client_state.client_service().apply_write_set(
                        wsrep::const_buffer()) ||
                    client_state.client_service().commit(ws_handle, ws_meta);
            }
        }
        else
        {
            // SR fragment applying not implemented yet
            assert(0);
        }
        if (not_replaying)
        {
            assert(txc.active() == false);
        }
        return ret;
    }

    int apply_toi(wsrep::server_service&,
                  wsrep::client_state& client_state,
                  const wsrep::ws_meta& ws_meta,
                  const wsrep::const_buffer& data)
    {
        if (wsrep::starts_transaction(ws_meta.flags()) &&
            wsrep::commits_transaction(ws_meta.flags()))
        {
            // Regular toi
            client_state.enter_toi(ws_meta);
            int ret(client_state.client_service().apply_toi(data));
            client_state.leave_toi();
            return ret;
        }
        else if (wsrep::starts_transaction(ws_meta.flags()))
        {
            // NBO begin
            throw wsrep::not_implemented_error();
        }
        else if (wsrep::commits_transaction(ws_meta.flags()))
        {
            // NBO end
            throw wsrep::not_implemented_error();
        }
        else
        {
            assert(0);
            return 0;
        }
    }

}

int wsrep::server_state::load_provider(const std::string& provider_spec,
                                         const std::string& provider_options)
{
    wsrep::log_info() << "Loading provider " << provider_spec;
    provider_ = wsrep::provider::make_provider(*this, provider_spec, provider_options);
    return (provider_ ? 0 : 1);
}

void wsrep::server_state::unload_provider()
{
    delete provider_;
    provider_ = 0;
}

int wsrep::server_state::connect(const std::string& cluster_name,
                                   const std::string& cluster_address,
                                   const std::string& state_donor,
                                   bool bootstrap)
{
    return provider().connect(cluster_name, cluster_address, state_donor,
                              bootstrap);
}

int wsrep::server_state::disconnect()
{
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        state(lock, s_disconnecting);
    }
    return provider().disconnect();
}

wsrep::server_state::~server_state()
{
    delete provider_;
}


wsrep::seqno wsrep::server_state::pause()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (pause_seqno_.is_undefined() == false)
    {
        throw wsrep::runtime_error("Trying to pause already paused provider");
    }
    if (state_ == s_synced)
    {
        if (desync(lock))
        {
            return wsrep::seqno::undefined();
        }
        desynced_on_pause_ = true;
    }
    pause_seqno_ = provider_->pause();
    if (pause_seqno_.is_undefined())
    {
        resync();
    }
    return pause_seqno_;
}

void wsrep::server_state::resume()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(pause_seqno_.is_undefined() == false);
    if (desynced_on_pause_)
    {
        resync(lock);
        desynced_on_pause_ = false;
    }
    if (provider_->resume())
    {
        throw wsrep::runtime_error("Failed to resume provider");
    }
    pause_seqno_ = wsrep::seqno::undefined();
}

std::string wsrep::server_state::prepare_for_sst()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joiner);
    return server_service_.sst_request();
}

int wsrep::server_state::start_sst(const std::string& sst_request,
                                   const wsrep::gtid& gtid,
                                   bool bypass)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_donor);
    int ret(0);
    lock.unlock();
    if (server_service_.start_sst(sst_request, gtid, bypass))
    {
        lock.lock();
        wsrep::log_warning() << "SST start failed";
        state(lock, s_synced);
        ret = 1;
    }
    return ret;
}

void wsrep::server_state::sst_sent(const wsrep::gtid& gtid, int error)
{
    wsrep::log_info() << "SST sent: " << gtid << ": " << error;
    if (provider_->sst_sent(gtid, error))
    {
        server_service_.log_message(wsrep::log::warning,
                                    "SST sent returned an error");
    }
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joined);
}

void wsrep::server_state::sst_transferred(const wsrep::gtid& gtid)
{
    wsrep::log_info() << "SST transferred";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    sst_gtid_ = gtid;
    if (server_service_.sst_before_init())
    {
        state(lock, s_initializing);
    }
    else
    {
        state(lock, s_joined);
    }
}

void wsrep::server_state::sst_received(const wsrep::gtid& gtid, int error)
{
    wsrep::log_info() << "SST received: " << gtid << ": " << error;
    if (provider_->sst_received(gtid, error))
    {
        throw wsrep::runtime_error("SST received failed");
    }
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joined);
}

void wsrep::server_state::initialized()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_info() << "Server initialized";
    init_initialized_ = true;
    if (sst_gtid_.is_undefined() == false &&
        server_service_.sst_before_init())
    {
        lock.unlock();
        if (provider().sst_received(sst_gtid_, 0))
        {
            throw wsrep::runtime_error("SST received failed");
        }
        lock.lock();
        sst_gtid_ = wsrep::gtid::undefined();
    }
    state(lock, s_initialized);
}

void wsrep::server_state::wait_until_state(
    wsrep::unique_lock<wsrep::mutex>& lock,
    enum wsrep::server_state::state state) const
{
    ++state_waiters_[state];
    while (state_ != state)
    {
        cond_.wait(lock);
    }
    --state_waiters_[state];
    cond_.notify_all();
}

void wsrep::server_state::last_committed_gtid(const wsrep::gtid& gtid)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(last_committed_gtid_.is_undefined() ||
           last_committed_gtid_.seqno() + 1 == gtid.seqno());
    last_committed_gtid_ = gtid;
    cond_.notify_all();
}

wsrep::gtid wsrep::server_state::last_committed_gtid() const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    return last_committed_gtid_;
}

int wsrep::server_state::wait_for_gtid(const wsrep::gtid& gtid) const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (gtid.id() != last_committed_gtid_.id())
    {
        return 1;
    }
    while (last_committed_gtid_.seqno() < gtid.seqno())
    {
        cond_.wait(lock);
    }
    return 0;
}

enum wsrep::provider::status
wsrep::server_state::causal_read(int timeout) const
{
    return provider_->causal_read(timeout);
}

void wsrep::server_state::on_connect()
{
    wsrep::log_info() << "Server " << name_ << " connected to cluster";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_connected);
}

void wsrep::server_state::on_view(const wsrep::view& view)
{
    wsrep::log_info()
        << "================================================\nView:\n"
        << "  id: " << view.state_id() << "\n"
        << "  status: " << view.status() << "\n"
        << "  own_index: " << view.own_index() << "\n"
        << "  final: " << view.final() << "\n"
        << "  members";
    const std::vector<wsrep::view::member>& members(view.members());
    for (std::vector<wsrep::view::member>::const_iterator i(members.begin());
         i != members.end(); ++i)
    {
        wsrep::log_info() << "    id: " << i->id() << " "
                          << "name: " << i->name();
    }
    wsrep::log_info() << "=================================================";
    if (view.status() == wsrep::view::primary)
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        if (state_ == s_connected && view.members().size() == 1)
        {
            state(lock, s_joiner);
            state(lock, s_initializing);
        }

        if (init_initialized_ == false)
        {
            // DBMS has not been initialized yet
            wait_until_state(lock, s_initialized);
        }

        assert(init_initialized_);

        if (state_ == s_initialized)
        {
            state(lock, s_joined);
            if (init_synced_)
            {
                state(lock, s_synced);
            }
        }

        if (view.final())
        {
            state(lock, s_disconnected);
        }
        current_view_ = view;
    }
}

void wsrep::server_state::on_sync()
{
    wsrep::log_info() << "Server " << name_ << " synced with group";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);

    if (server_service_.sst_before_init())
    {
        switch (state_)
        {
        case s_connected:
            state(lock, s_joiner);
            // fall through
        case s_joiner:
            state(lock, s_initializing);
            break;
        case s_initialized:
            state(lock, s_joined);
            // fall through
        default:
            /* State */
            state(lock, s_synced);
        };
    }
    init_synced_ = true;
}

int wsrep::server_state::on_apply(
    wsrep::client_state& client_state,
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    const wsrep::const_buffer& data)
{
    if (is_toi(ws_meta.flags()))
    {
        return apply_toi(server_service_, client_state, ws_meta, data);
    }
    else if (is_commutative(ws_meta.flags()) || is_native(ws_meta.flags()))
    {
        // Not implemented yet.
        assert(0);
        return 0;
    }
    else
    {
        return apply_write_set(*this, client_state,
                               ws_handle, ws_meta, data);
    }
}

void wsrep::server_state::start_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id,
    wsrep::client_state* client_state)
{
    if (streaming_appliers_.insert(
            std::make_pair(std::make_pair(server_id, transaction_id),
                           client_state)).second == false)
    {
        wsrep::log_error() << "Could not insert streaming applier";
        delete client_state;
        throw wsrep::fatal_error();
    }
}

void wsrep::server_state::stop_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id)
{
    streaming_appliers_map::iterator i(
        streaming_appliers_.find(std::make_pair(server_id, transaction_id)));
    assert(i != streaming_appliers_.end());
    if (i == streaming_appliers_.end())
    {
        wsrep::log_warning() << "Could not find streaming applier for "
                             << server_id << ":" << transaction_id;
    }
    else
    {
        streaming_appliers_.erase(i);
    }
}

wsrep::client_state* wsrep::server_state::find_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id) const
{
    streaming_appliers_map::const_iterator i(
        streaming_appliers_.find(std::make_pair(server_id, transaction_id)));
    return (i == streaming_appliers_.end() ? 0 : i->second);
}

// Private


int wsrep::server_state::desync(wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    ++desync_count_;
    if (state_ == s_synced)
    {
        assert(desync_count_ == 0);
        return provider_->desync();
    }
    return 0;
}

void wsrep::server_state::resync(wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    --desync_count_;
    if (desync_count_ == 0)
    {
        if (provider_->resync())
        {
            throw wsrep::runtime_error("Failed to resync");
        }
    }
}


void wsrep::server_state::state(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum wsrep::server_state::state state)
{
    assert(lock.owns_lock());
    static const char allowed[n_states_][n_states_] =
        {
            /* dis, ing, ized, cted, jer, jed, dor, sed, ding */
            {  0,   1,   0,    1,    0,   0,   0,   0,   0}, /* dis */
            {  0,   0,   1,    0,    0,   0,   0,   0,   0}, /* ing */
            {  0,   0,   0,    1,    0,   1,   0,   0,   0}, /* ized */
            {  0,   0,   0,    0,    1,   0,   0,   0,   0}, /* cted */
            {  0,   1,   0,    0,    0,   1,   0,   0,   0}, /* jer */
            {  0,   0,   0,    0,    0,   0,   0,   1,   1}, /* jed */
            {  0,   0,   0,    0,    0,   1,   0,   0,   1}, /* dor */
            {  0,   0,   0,    0,    0,   1,   1,   0,   1}, /* sed */
            {  1,   0,   0,    0,    0,   0,   0,   0,   0}  /* ding */
        };

    if (allowed[state_][state])
    {
        wsrep::log_info() << "server " << name_ << " state change: "
                          << to_c_string(state_) << " -> "
                          << to_c_string(state);
        state_hist_.push_back(state_);
        state_ = state;
        cond_.notify_all();
        while (state_waiters_[state_])
        {
            cond_.wait(lock);
        }
    }
    else
    {
        std::ostringstream os;
        os << "server: " << name_ << " unallowed state transition: "
           << wsrep::to_string(state_) << " -> " << wsrep::to_string(state);
        wsrep::log_error() << os.str() << "\n";
        ::abort();
        // throw wsrep::runtime_error(os.str());
    }
}
