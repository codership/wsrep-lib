//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/server_state.hpp"
#include "wsrep/server_service.hpp"
#include "wsrep/high_priority_service.hpp"
#include "wsrep/transaction.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/id.hpp"

#include <cassert>
#include <sstream>

namespace
{
    //
    // This method is used to deal with historical burden of several
    // ways to bootstrap the cluster. Bootstrap happens if
    //
    // * bootstrap option is given
    // * cluster_address is "gcomm://" (Galera provider)
    //
    bool is_bootstrap(const std::string& cluster_address, bool bootstrap)
    {
        return (bootstrap || cluster_address == "gcomm://");
    }

    int apply_fragment(wsrep::server_state& server_state,
                       wsrep::high_priority_service& high_priority_service,
                       wsrep::high_priority_service& streaming_applier,
                       const wsrep::ws_handle& ws_handle,
                       const wsrep::ws_meta& ws_meta,
                       const wsrep::const_buffer& data)
    {
        int ret;
        {
            wsrep::high_priority_switch sw(high_priority_service,
                                           streaming_applier);
            ret = streaming_applier.apply_write_set(ws_meta, data);
            streaming_applier.after_apply();
        }
        ret = ret || high_priority_service.append_fragment_and_commit(
            ws_handle, ws_meta, data);
        high_priority_service.after_apply();
        return ret;
    }


    int commit_fragment(wsrep::server_state& server_state,
                        wsrep::high_priority_service& high_priority_service,
                        wsrep::high_priority_service* streaming_applier,
                        const wsrep::ws_handle& ws_handle,
                        const wsrep::ws_meta& ws_meta,
                        const wsrep::const_buffer& data)
    {
        int ret;
        // Make high priority switch to go out of scope
        // before the streaming applier is released.
        {
            wsrep::high_priority_switch sw(
                high_priority_service, *streaming_applier);
            streaming_applier->remove_fragments(ws_meta);
            ret = streaming_applier->commit(ws_handle, ws_meta);
            streaming_applier->after_apply();
        }
        server_state.stop_streaming_applier(
            ws_meta.server_id(), ws_meta.transaction_id());
        server_state.server_service().release_high_priority_service(
            streaming_applier);
        return ret;
    }

    int rollback_fragment(wsrep::server_state& server_state,
                          wsrep::high_priority_service& high_priority_service,
                          wsrep::high_priority_service* streaming_applier,
                          const wsrep::ws_handle& ws_handle,
                          const wsrep::ws_meta& ws_meta,
                          const wsrep::const_buffer& data)
    {
        int ret= 0;
        // Adopts transaction state and starts a transaction for
        // high priority service
        high_priority_service.adopt_transaction(
            streaming_applier->transaction());
        {
            wsrep::high_priority_switch ws(
                high_priority_service, *streaming_applier);
            streaming_applier->rollback();
            streaming_applier->after_apply();
        }
        server_state.stop_streaming_applier(
            ws_meta.server_id(), ws_meta.transaction_id());
        server_state.server_service().release_high_priority_service(
            streaming_applier);

        high_priority_service.remove_fragments(ws_meta);
        high_priority_service.commit(ws_handle, ws_meta);
        high_priority_service.after_apply();
        return ret;
    }

    int apply_write_set(wsrep::server_state& server_state,
                        wsrep::high_priority_service& high_priority_service,
                        const wsrep::ws_handle& ws_handle,
                        const wsrep::ws_meta& ws_meta,
                        const wsrep::const_buffer& data)
    {
        int ret(0);
        if (wsrep::starts_transaction(ws_meta.flags()) &&
                 wsrep::commits_transaction(ws_meta.flags()) &&
                 wsrep::rolls_back_transaction(ws_meta.flags()))
        {
            // Non streaming rollback (certification failed)
            ret = high_priority_service.log_dummy_write_set(
                ws_handle, ws_meta);
        }
        else if (wsrep::starts_transaction(ws_meta.flags()) &&
            wsrep::commits_transaction(ws_meta.flags()))
        {
            if (high_priority_service.start_transaction(ws_handle, ws_meta))
            {
                ret = 1;
            }
            else if (high_priority_service.apply_write_set(ws_meta, data))
            {
                ret = 1;
            }
            else if (high_priority_service.commit(ws_handle, ws_meta))
            {
                ret = 1;
            }
            if (ret)
            {
                high_priority_service.rollback();
            }
            high_priority_service.after_apply();
        }
        else if (wsrep::starts_transaction(ws_meta.flags()))
        {
            assert(server_state.find_streaming_applier(
                       ws_meta.server_id(), ws_meta.transaction_id()) == 0);
            wsrep::high_priority_service* sa(
                server_state.server_service().streaming_applier_service(
                    high_priority_service));
            server_state.start_streaming_applier(
                ws_meta.server_id(), ws_meta.transaction_id(), sa);
            sa->start_transaction(ws_handle, ws_meta);
            ret = apply_fragment(server_state,
                                 high_priority_service,
                                 *sa,
                                 ws_handle,
                                 ws_meta,
                                 data);
        }
        else if (ws_meta.flags() == 0)
        {
            wsrep::high_priority_service* sa(
                server_state.find_streaming_applier(
                    ws_meta.server_id(), ws_meta.transaction_id()));
            if (sa == 0)
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
                ret = apply_fragment(server_state,
                                     high_priority_service,
                                     *sa,
                                     ws_handle,
                                     ws_meta,
                                     data);
            }
        }
        else if (wsrep::commits_transaction(ws_meta.flags()))
        {
            if (high_priority_service.is_replaying())
            {
                ret = high_priority_service.apply_write_set(ws_meta, data) ||
                    high_priority_service.commit(ws_handle, ws_meta);
            }
            else
            {
                wsrep::high_priority_service* sa(
                    server_state.find_streaming_applier(
                        ws_meta.server_id(), ws_meta.transaction_id()));
                if (sa == 0)
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
                    // Commit fragment consumes sa
                    ret = commit_fragment(server_state,
                                          high_priority_service,
                                          sa,
                                          ws_handle,
                                          ws_meta,
                                          data);
                }
            }
        }
        else if (wsrep::rolls_back_transaction(ws_meta.flags()))
        {
            wsrep::high_priority_service* sa(
                server_state.find_streaming_applier(
                    ws_meta.server_id(), ws_meta.transaction_id()));
            if (sa == 0)
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
                ret = high_priority_service.log_dummy_write_set(
                    ws_handle, ws_meta);
            }
            else
            {
                // Rollback fragment consumes sa
                ret = rollback_fragment(server_state,
                                        high_priority_service,
                                        sa,
                                        ws_handle,
                                        ws_meta,
                                        data);
            }
        }
        else
        {
            assert(0);
        }
        return ret;
    }

    int apply_toi(wsrep::provider& provider,
                  wsrep::high_priority_service& high_priority_service,
                  const wsrep::ws_handle& ws_handle,
                  const wsrep::ws_meta& ws_meta,
                  const wsrep::const_buffer& data)
    {
        if (wsrep::starts_transaction(ws_meta.flags()) &&
            wsrep::commits_transaction(ws_meta.flags()))
        {
            //
            // Regular TOI.
            //
            // Note that we ignore error returned by apply_toi
            // call here. This must be revised after the error
            // voting is added.
            //
            provider.commit_order_enter(ws_handle, ws_meta);
            (void)high_priority_service.apply_toi(ws_meta, data);
            provider.commit_order_leave(ws_handle, ws_meta);
            return 0;
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
    wsrep::log_info() << "Loading provider "
                      << provider_spec
                      << "initial position: "
                      << initial_position_;
    provider_ = wsrep::provider::make_provider(
        *this, provider_spec, provider_options);
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
    bootstrap_ = is_bootstrap(cluster_address, bootstrap);
    wsrep::log_info() << "Connecting with bootstrap option: " << bootstrap_;
    return provider().connect(cluster_name, cluster_address, state_donor,
                              bootstrap_);
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

std::vector<wsrep::provider::status_variable>
wsrep::server_state::status() const
{
    return provider_->status();
}


wsrep::seqno wsrep::server_state::pause()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    // Disallow concurrent calls to pause to in order to have non-concurrent
    // access to desynced_on_pause_ which is checked in resume() call.
    wsrep::log_info() << "pause";
    while (pause_count_ > 0)
    {
        cond_.wait(lock);
    }
    ++pause_count_;
    assert(pause_seqno_.is_undefined());
    lock.unlock();
    pause_seqno_ = provider_->pause();
    lock.lock();
    if (pause_seqno_.is_undefined())
    {
        --pause_count_;
    }
    return pause_seqno_;
}

void wsrep::server_state::resume()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_info() << "resume";
    assert(pause_seqno_.is_undefined() == false);
    assert(pause_count_ == 1);
    if (provider_->resume())
    {
        throw wsrep::runtime_error("Failed to resume provider");
    }
    pause_seqno_ = wsrep::seqno::undefined();
    --pause_count_;
    cond_.notify_all();
}

wsrep::seqno wsrep::server_state::desync_and_pause()
{
    wsrep::log_info() << "desync_and_pause";
    if (desync())
    {
        wsrep::log_warning() << "Failed to desync server";
        return wsrep::seqno::undefined();
    }
    wsrep::seqno ret(pause());
    if (ret.is_undefined())
    {
        wsrep::log_warning() << "Failed to pause provider";
        resync();
        return wsrep::seqno::undefined();
    }
    wsrep::log_info() << "Provider paused at: " << ret;
    return ret;
}

void wsrep::server_state::resume_and_resync()
{
    wsrep::log_info() << "resume_and_resync";
    try
    {
        resume();
        resync();
    }
    catch (const wsrep::runtime_error& e)
    {
        wsrep::log_warning()
            << "Resume and resync failed, server may have to be restarted";
    }
}

std::string wsrep::server_state::prepare_for_sst()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joiner);
    lock.unlock();
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
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joined);
    lock.unlock();
    if (provider_->sst_sent(gtid, error))
    {
        server_service_.log_message(wsrep::log::warning,
                                    "SST sent returned an error");
    }
}

void wsrep::server_state::sst_received(const wsrep::gtid& gtid, int error)
{
    wsrep::log_info() << "SST received: " << gtid;
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state_ == s_joiner || state_ == s_initialized);
    if (server_service_.sst_before_init())
    {
        if (init_initialized_ == false)
        {
            state(lock, s_initializing);
            wait_until_state(lock, s_initialized);
            assert(init_initialized_);
        }
        state(lock, s_joined);
        lock.unlock();
        if (provider().sst_received(gtid, error))
        {
            throw wsrep::runtime_error("SST received failed");
        }
    }
    else
    {
        state(lock, s_joined);
        if (provider().sst_received(gtid, error))
        {
            throw wsrep::runtime_error("SST received failed");
        }
    }
}

void wsrep::server_state::initialized()
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_info() << "Server initialized";
    init_initialized_ = true;
    if (server_service_.sst_before_init())
    {
        state(lock, s_initialized);
    }
    else
    {
        state(lock, s_initializing);
        state(lock, s_initialized);
    }
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

enum wsrep::provider::status
wsrep::server_state::wait_for_gtid(const wsrep::gtid& gtid, int timeout)
    const
{
    return provider_->wait_for_gtid(gtid, timeout);
}

std::pair<wsrep::gtid, enum wsrep::provider::status>
wsrep::server_state::causal_read(int timeout) const
{
    return provider_->causal_read(timeout);
}

void wsrep::server_state::on_connect(const wsrep::gtid& gtid)
{
    wsrep::log_info() << "Server "
                      << name_
                      << " connected to cluster at position "
                      << gtid;
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    connected_gtid_ = gtid;
    state(lock, s_connected);
}

void wsrep::server_state::on_view(const wsrep::view& view)
{
    wsrep::log_info()
        << "================================================\nView:\n"
        << "  id: " << view.state_id() << "\n"
        << "  status: " << view.status() << "\n"
        << "  prococol_version: " << view.protocol_version() << "\n"
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
    server_service_.log_view(view);
    current_view_ = view;
    if (view.status() == wsrep::view::primary)
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        if (view.own_index() >= 0)
        {
            if (id_.is_undefined())
            {
                // No identifier was passed during server state initialization
                // and the ID was generated by the provider.
                id_ = view.members()[view.own_index()].id();
            }
            else
            {
                // Own identifier must not change between views.
                // assert(id_ == view.members()[view.own_index()].id());
            }
        }
        assert(view.final() == false);

        //
        // Reached primary from connected state. This may mean the following
        //
        // 1) Server was joined to the cluster and got SST transfer
        // 2) Server was partitioned from the cluster and got back
        // 3) A new cluster was bootstrapped from non-prim cluster
        //
        // There is no enough information here what was the cause
        // of the primary component, so we need to walk through
        // all states leading to joined to notify possible state
        // waiters in other threads.
        //
        if (server_service_.sst_before_init())
        {
            if (state_ == s_connected)
            {
                state(lock, s_joiner);
                state(lock, s_initializing);
            }
            else if (state_ == s_joiner)
            {
                // Got partiioned from the cluster, got IST and
                // started applying actions.
                state(lock, s_joined);
            }
        }
        else
        {
            if (state_ == s_connected)
            {
                state(lock, s_joiner);
            }
        }

        if (init_initialized_ == false)
        {
            lock.unlock();
            server_service_.debug_sync("on_view_wait_initialized");
            lock.lock();
            wait_until_state(lock, s_initialized);
        }
        assert(init_initialized_);

        if (bootstrap_)
        {
            server_service_.bootstrap();
            bootstrap_ = false;
        }

        if (server_service_.sst_before_init())
        {
            if (state_ == s_initialized)
            {
                state(lock, s_joined);
                if (init_synced_)
                {
                    state(lock, s_synced);
                }
            }
        }
        else
        {
            if (state_ == s_joiner)
            {
                state(lock, s_joined);
                if (init_synced_)
                {
                    state(lock, s_synced);
                }
            }
        }
    }
    else if (view.status() == wsrep::view::non_primary)
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        wsrep::log_info() << "Non-primary view";
        if (view.final())
        {
            state(lock, s_disconnected);
        }
        else if (state_ != s_disconnecting)
        {
            state(lock, s_connected);
        }
    }
    else
    {
        assert(view.final());
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        state(lock, s_disconnected);
        id_ = wsrep::id::undefined();
    }
}

void wsrep::server_state::on_sync()
{
    wsrep::log_info() << "Server " << name_ << " synced with group";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);

    // Initial sync
    if (server_service_.sst_before_init() && init_synced_ == false)
    {
        switch (state_)
        {
        case s_synced:
            break;
        case s_connected:
            state(lock, s_joiner);
            // fall through
        case s_joiner:
            state(lock, s_initializing);
            break;
        case s_donor:
            state(lock, s_joined);
            state(lock, s_synced);
            break;
        case s_initialized:
            state(lock, s_joined);
            // fall through
        default:
            /* State */
            state(lock, s_synced);
        };
    }
    else
    {
        // Calls to on_sync() in synced state are possible if
        // server desyncs itself from the group. Provider does not
        // inform about this through callbacks.
        if (state_ != s_synced)
        {
            state(lock, s_synced);
        }
    }
    init_synced_ = true;
}

int wsrep::server_state::on_apply(
    wsrep::high_priority_service& high_priority_service,
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    const wsrep::const_buffer& data)
{
    if (is_toi(ws_meta.flags()))
    {
        return apply_toi(provider(), high_priority_service,
                         ws_handle, ws_meta, data);
    }
    else if (is_commutative(ws_meta.flags()) || is_native(ws_meta.flags()))
    {
        // Not implemented yet.
        assert(0);
        return 0;
    }
    else
    {
        return apply_write_set(*this, high_priority_service,
                               ws_handle, ws_meta, data);
    }
}

void wsrep::server_state::start_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id,
    wsrep::high_priority_service* sa)
{
    if (streaming_appliers_.insert(
            std::make_pair(std::make_pair(server_id, transaction_id),
                           sa)).second == false)
    {
        wsrep::log_error() << "Could not insert streaming applier";
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

wsrep::high_priority_service* wsrep::server_state::find_streaming_applier(
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
    lock.unlock();
    int ret(provider_->desync());
    lock.lock();
    if (ret)
    {
        --desync_count_;
    }
    return ret;
}

void wsrep::server_state::resync(wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    assert(desync_count_ > 0);
    --desync_count_;
    if (provider_->resync())
    {
        throw wsrep::runtime_error("Failed to resync");
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
            {  1,   0,   1,    0,    0,   0,   0,   0,   0}, /* ing */
            {  1,   0,   0,    1,    0,   1,   0,   0,   0}, /* ized */
            {  1,   0,   0,    1,    1,   0,   0,   1,   0}, /* cted */
            {  1,   1,   0,    0,    0,   1,   0,   0,   0}, /* jer */
            {  1,   0,   0,    1,    0,   0,   0,   1,   1}, /* jed */
            {  1,   0,   0,    0,    0,   1,   0,   0,   1}, /* dor */
            {  1,   0,   0,    1,    0,   1,   1,   0,   1}, /* sed */
            {  1,   0,   0,    0,    0,   0,   0,   0,   0}  /* ding */
        };

    if (allowed[state_][state])
    {
        wsrep::log_debug() << "server " << name_ << " state change: "
                           << to_c_string(state_) << " -> "
                           << to_c_string(state);
        state_hist_.push_back(state_);
        server_service_.log_state_change(state_, state);
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
