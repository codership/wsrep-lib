/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "wsrep/server_state.hpp"
#include "wsrep/client_state.hpp"
#include "wsrep/server_service.hpp"
#include "wsrep/high_priority_service.hpp"
#include "wsrep/transaction.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"
#include "wsrep/id.hpp"

#include <cassert>
#include <sstream>
#include <algorithm>

//////////////////////////////////////////////////////////////////////////////
//                               Helpers                                    //
//////////////////////////////////////////////////////////////////////////////


//
// This method is used to deal with historical burden of several
// ways to bootstrap the cluster. Bootstrap happens if
//
// * bootstrap option is given
// * cluster_address is "gcomm://" (Galera provider)
//
static bool is_bootstrap(const std::string& cluster_address, bool bootstrap)
{
    return (bootstrap || cluster_address == "gcomm://");
}

static int apply_fragment(wsrep::high_priority_service& high_priority_service,
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
    high_priority_service.debug_crash("crash_apply_cb_before_append_frag");
    ret = ret || high_priority_service.append_fragment_and_commit(
        ws_handle, ws_meta, data);
    high_priority_service.debug_crash("crash_apply_cb_after_append_frag");
    high_priority_service.after_apply();
    return ret;
}


static int commit_fragment(wsrep::server_state& server_state,
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
        ret = streaming_applier->apply_write_set(ws_meta, data);
        streaming_applier->debug_crash(
            "crash_apply_cb_before_fragment_removal");
        ret = ret || streaming_applier->remove_fragments(ws_meta);
        streaming_applier->debug_crash(
            "crash_apply_cb_after_fragment_removal");
        streaming_applier->debug_crash(
            "crash_commit_cb_before_last_fragment_commit");
        ret = ret || streaming_applier->commit(ws_handle, ws_meta);
        streaming_applier->debug_crash(
            "crash_commit_cb_last_fragment_commit_success");
        streaming_applier->after_apply();
    }
    if (ret == 0)
    {
        server_state.stop_streaming_applier(
            ws_meta.server_id(), ws_meta.transaction_id());
        server_state.server_service().release_high_priority_service(
            streaming_applier);
    }
    return ret;
}

static int rollback_fragment(wsrep::server_state& server_state,
                             wsrep::high_priority_service& high_priority_service,
                             wsrep::high_priority_service* streaming_applier,
                             const wsrep::ws_handle& ws_handle,
                             const wsrep::ws_meta& ws_meta)
{
    int ret= 0;
    // Adopts transaction state and starts a transaction for
    // high priority service
    high_priority_service.adopt_transaction(
        streaming_applier->transaction());
    {
        wsrep::high_priority_switch ws(
            high_priority_service, *streaming_applier);
        // Streaming applier rolls back out of order. Fragment
        // removal grabs commit order below.
        streaming_applier->rollback(wsrep::ws_handle(), wsrep::ws_meta());
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

static int apply_write_set(wsrep::server_state& server_state,
                           wsrep::high_priority_service& high_priority_service,
                           const wsrep::ws_handle& ws_handle,
                           const wsrep::ws_meta& ws_meta,
                           const wsrep::const_buffer& data)
{
    int ret(0);
    // wsrep::log_info() << "apply_write_set: " << ws_meta;
    if (wsrep::rolls_back_transaction(ws_meta.flags()))
    {
        if (wsrep::starts_transaction(ws_meta.flags()))
        {
            // No transaction existed before, log a dummy write set
            ret = high_priority_service.log_dummy_write_set(
                ws_handle, ws_meta);
        }
        else
        {
            wsrep::high_priority_service* sa(
                server_state.find_streaming_applier(
                    ws_meta.server_id(), ws_meta.transaction_id()));
            if (sa == 0)
            {
                // It is a known limitation that galera provider
                // cannot always determine if certification test
                // for interrupted transaction will pass or fail
                // (see comments in transaction::certify_fragment()).
                // As a consequence, unnecessary rollback fragments
                // may be delivered here. The message below has
                // been intentionally turned into a debug message,
                // rather than warning.
                wsrep::log_debug()
                    << "Could not find applier context for "
                    << ws_meta.server_id()
                    << ": " << ws_meta.transaction_id();
                ret = high_priority_service.log_dummy_write_set(
                    ws_handle, ws_meta);
            }
            else
            {
                // rollback_fragment() consumes sa
                ret = rollback_fragment(server_state,
                                        high_priority_service,
                                        sa,
                                        ws_handle,
                                        ws_meta);
            }
        }
    }
    else if (wsrep::starts_transaction(ws_meta.flags()) &&
             wsrep::commits_transaction(ws_meta.flags()))
    {
        ret = high_priority_service.start_transaction(ws_handle, ws_meta) ||
            high_priority_service.apply_write_set(ws_meta, data) ||
            high_priority_service.commit(ws_handle, ws_meta);
        if (ret)
        {
            high_priority_service.rollback(ws_handle, ws_meta);
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
        ret = apply_fragment(high_priority_service,
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
            ret = high_priority_service.log_dummy_write_set(
                ws_handle, ws_meta);
        }
        else
        {
            ret = apply_fragment(high_priority_service,
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
            ret = high_priority_service.start_transaction(
                ws_handle, ws_meta) ||
                high_priority_service.apply_write_set(ws_meta, data) ||
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
                ret = high_priority_service.log_dummy_write_set(
                    ws_handle, ws_meta);
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
    else
    {
        assert(0);
    }
    if (ret)
    {
        wsrep::log_info() << "Failed to apply write set: " << ws_meta;
    }
    return ret;
}

static int apply_toi(wsrep::provider& provider,
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

//////////////////////////////////////////////////////////////////////////////
//                            Server State                                  //
//////////////////////////////////////////////////////////////////////////////

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
        interrupt_state_waiters(lock);
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
    return provider().status();
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
    pause_seqno_ = provider().pause();
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
    if (provider().resume())
    {
        throw wsrep::runtime_error("Failed to resume provider");
    }
    pause_seqno_ = wsrep::seqno::undefined();
    --pause_count_;
    cond_.notify_all();
}

wsrep::seqno wsrep::server_state::desync_and_pause()
{
    wsrep::log_info() << "Desyncing and pausing the provider";
    // Temporary variable to store desync() return status. This will be
    // assigned to desynced_on_pause_ after pause() call to prevent
    // concurrent access to  member variable desynced_on_pause_.
    bool desync_successful;
    if (desync())
    {
        // Desync may give transient error if the provider cannot
        // communicate with the rest of the cluster. However, this
        // error can be tolerated because if the provider can be
        // paused succesfully below.
        wsrep::log_debug() << "Failed to desync server before pause";
        desync_successful = false;
    }
    else
    {
        desync_successful = true;
    }
    wsrep::seqno ret(pause());
    if (ret.is_undefined())
    {
        wsrep::log_warning() << "Failed to pause provider";
        resync();
        return wsrep::seqno::undefined();
    }
    else
    {
        desynced_on_pause_ = desync_successful;
    }
    wsrep::log_info() << "Provider paused at: " << ret;
    return ret;
}

void wsrep::server_state::resume_and_resync()
{
    wsrep::log_info() << "Resuming and resyncing the provider";
    try
    {
        // Assign desynced_on_pause_ to local variable before resuming
        // in order to avoid concurrent access to desynced_on_pause_ member
        // variable.
        bool do_resync = desynced_on_pause_;
        desynced_on_pause_ = false;
        resume();
        if (do_resync)
        {
            resync();
        }
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
    if (0 == error)
        wsrep::log_info() << "SST sent: " << gtid;
    else
        wsrep::log_info() << "SST sending failed: " << error;

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_joined);
    lock.unlock();
    if (provider().sst_sent(gtid, error))
    {
        server_service_.log_message(wsrep::log::warning,
                                    "Provider sst_sent() returned an error");
    }
}

void wsrep::server_state::sst_received(wsrep::client_service& cs,
                                       int const error)
{
    wsrep::log_info() << "SST received";
    wsrep::gtid gtid(wsrep::gtid::undefined());
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    assert(state_ == s_joiner || state_ == s_initialized);

    // Run initialization only if the SST was successful.
    // In case of SST failure the system is in undefined state
    // may not be recoverable.
    if (error == 0)
    {
        if (server_service_.sst_before_init())
        {
            if (init_initialized_ == false)
            {
                state(lock, s_initializing);
                lock.unlock();
                server_service_.debug_sync("on_view_wait_initialized");
                lock.lock();
                wait_until_state(lock, s_initialized);
                assert(init_initialized_);
            }
        }
        state(lock, s_joined);
        lock.unlock();

        if (id_.is_undefined())
        {
            assert(0);
            throw wsrep::runtime_error(
                "wsrep::sst_received() called before connection to cluster");
        }

        gtid = server_service_.get_position(cs);
        wsrep::log_info() << "Recovered position from storage: " << gtid;
        wsrep::view const v(server_service_.get_view(cs, id_));
        wsrep::log_info() << "Recovered view from SST:\n" << v;

        if (v.state_id().id() != gtid.id() ||
            v.state_id().seqno() > gtid.seqno())
        {
            /* Since IN GENERAL we may not be able to recover SST GTID from
             * the state data, we have to rely on SST script passing the GTID
             * value explicitly.
             * Here we check if the passed GTID makes any sense: it should
             * have the same UUID and greater or equal seqno than the last
             * logged view. */
            std::ostringstream msg;
            msg << "SST script passed bogus GTID: " << gtid
                << ". Preceeding view GTID: " << v.state_id();
            throw wsrep::runtime_error(msg.str());
        }

        current_view_ = v;
        server_service_.log_view(NULL /* this view is stored already */, v);
    }

    if (provider().sst_received(gtid, error))
    {
        throw wsrep::runtime_error("wsrep::sst_received() failed");
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
    return provider().wait_for_gtid(gtid, timeout);
}

int 
wsrep::server_state::set_encryption_key(std::vector<unsigned char>& key)
{
    encryption_key_ = key;
    if (state_ != s_disconnected)
    {
        return provider_->enc_set_key(wsrep::const_buffer(encryption_key_.data(),
                                                          encryption_key_.size()));
    }
    return 0;
}

std::pair<wsrep::gtid, enum wsrep::provider::status>
wsrep::server_state::causal_read(int timeout) const
{
    return provider().causal_read(timeout);
}

void wsrep::server_state::on_connect(const wsrep::view& view)
{
    // Sanity checks
    if (view.own_index() < 0 ||
        size_t(view.own_index()) >= view.members().size())
    {
        std::ostringstream os;
        os << "Invalid view on connect: own index out of range: " << view;
#ifndef NDEBUG
        wsrep::log_error() << os.str();
        assert(0);
#endif
        throw wsrep::runtime_error(os.str());
    }

    if (id_.is_undefined() == false &&
        id_ != view.members()[view.own_index()].id())
    {
        std::ostringstream os;
        os << "Connection in connected state.\n"
           << "Connected view:\n" << view
           << "Previous view:\n" << current_view_
           << "Current own ID: " << id_;
#ifndef NDEBUG
        wsrep::log_error() << os.str();
        assert(0);
#endif
        throw wsrep::runtime_error(os.str());
    }
    else
    {
        id_ = view.members()[view.own_index()].id();
    }

    wsrep::log_info() << "Server "
                      << name_
                      << " connected to cluster at position "
                      << view.state_id()
                      << " with ID "
                      << id_;

    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    connected_gtid_ = view.state_id();
    state(lock, s_connected);
}

void wsrep::server_state::on_primary_view(
    const wsrep::view& view WSREP_UNUSED,
    wsrep::high_priority_service* high_priority_service)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
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
            if (init_initialized_)
            {
                // If server side has already been initialized,
                // skip directly to s_joined.
                state(lock, s_initialized);
                state(lock, s_joined);
            }
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
        if (init_initialized_ && state_ != s_joined)
        {
            // If server side has already been initialized,
            // skip directly to s_joined.
            state(lock, s_joined);
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

    assert(high_priority_service);
    if (high_priority_service)
    {
        close_orphaned_sr_transactions(lock, *high_priority_service);
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

void wsrep::server_state::on_non_primary_view(
    const wsrep::view& view,
    wsrep::high_priority_service* high_priority_service)
{
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        wsrep::log_info() << "Non-primary view";
        if (view.final())
        {
            go_final(lock, view, high_priority_service);
        }
        else if (state_ != s_disconnecting)
        {
            state(lock, s_connected);
        }
}

void wsrep::server_state::go_final(wsrep::unique_lock<wsrep::mutex>& lock,
                                   const wsrep::view& view,
                                   wsrep::high_priority_service* hps)
{
    (void)view; // avoid compiler warning "unused parameter 'view'"
    assert(view.final());
    assert(hps);
    if (hps)
    {
        close_transactions_at_disconnect(*hps);
    }
    state(lock, s_disconnected);
    id_ = wsrep::id::undefined();
}

void wsrep::server_state::on_view(const wsrep::view& view,
                                  wsrep::high_priority_service* high_priority_service)
{
    wsrep::log_info()
        << "================================================\nView:\n"
        << "  id: " << view.state_id() << "\n"
        << "  seqno: " << view.view_seqno() << "\n"
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
    current_view_ = view;
    switch (view.status())
    {
    case wsrep::view::primary:
        on_primary_view(view, high_priority_service);
        break;
    case wsrep::view::non_primary:
        on_non_primary_view(view, high_priority_service);
        break;
    case wsrep::view::disconnected:
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        go_final(lock, view, high_priority_service);
        break;
    }
    default:
        wsrep::log_warning() << "Unrecognized view status: " << view.status();
        assert(0);
    }

    server_service_.log_view(high_priority_service, view);
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

void wsrep::server_state::start_streaming_client(
    wsrep::client_state* client_state)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_debug() << "Start streaming client: " << client_state->id();
    if (streaming_clients_.insert(
            std::make_pair(client_state->id(), client_state)).second == false)
    {
        wsrep::log_warning() << "Failed to insert streaming client "
                             << client_state->id();
        assert(0);
    }
}

void wsrep::server_state::convert_streaming_client_to_applier(
    wsrep::client_state* client_state)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_debug() << "Convert streaming client to applier "
                       << client_state->id();
    streaming_clients_map::iterator i(
        streaming_clients_.find(client_state->id()));
    assert(i != streaming_clients_.end());
    if (i == streaming_clients_.end())
    {
        wsrep::log_warning() << "Unable to find streaming client "
                             << client_state->id();
        assert(0);
    }
    else
    {
        streaming_clients_.erase(i);
    }
    wsrep::high_priority_service* streaming_applier(
        server_service_.streaming_applier_service(
            client_state->client_service()));
    streaming_applier->adopt_transaction(client_state->transaction());
    if (streaming_appliers_.insert(
            std::make_pair(
                std::make_pair(client_state->transaction().server_id(),
                               client_state->transaction().id()),
                streaming_applier)).second == false)
    {
        wsrep::log_warning() << "Could not insert streaming applier "
                             << id_
                             << ", "
                             << client_state->transaction().id();
        assert(0);
    }
}


void wsrep::server_state::stop_streaming_client(
    wsrep::client_state* client_state)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    wsrep::log_debug() << "Stop streaming client: " << client_state->id();
    streaming_clients_map::iterator i(
        streaming_clients_.find(client_state->id()));
    assert(i != streaming_clients_.end());
    if (i == streaming_clients_.end())
    {
        wsrep::log_warning() << "Unable to find streaming client "
                             << client_state->id();
        assert(0);
        return;
    }
    else
    {
        streaming_clients_.erase(i);
        cond_.notify_all();
    }
}

void wsrep::server_state::start_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id,
    wsrep::high_priority_service* sa)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
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
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
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
        cond_.notify_all();
    }
}

wsrep::high_priority_service* wsrep::server_state::find_streaming_applier(
    const wsrep::id& server_id,
    const wsrep::transaction_id& transaction_id) const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    streaming_appliers_map::const_iterator i(
        streaming_appliers_.find(std::make_pair(server_id, transaction_id)));
    return (i == streaming_appliers_.end() ? 0 : i->second);
}

//////////////////////////////////////////////////////////////////////////////
//                              Private                                     //
//////////////////////////////////////////////////////////////////////////////

int wsrep::server_state::desync(wsrep::unique_lock<wsrep::mutex>& lock)
{
    assert(lock.owns_lock());
    ++desync_count_;
    lock.unlock();
    int ret(provider().desync());
    lock.lock();
    if (ret)
    {
        --desync_count_;
    }
    return ret;
}

void wsrep::server_state::resync(wsrep::unique_lock<wsrep::mutex>&
                                 lock WSREP_UNUSED)
{
    assert(lock.owns_lock());
    assert(desync_count_ > 0);
    if (desync_count_ > 0)
    {
        --desync_count_;
        if (provider().resync())
        {
            throw wsrep::runtime_error("Failed to resync");
        }
    }
    else
    {
        wsrep::log_warning() << "desync_count " << desync_count_
                             << " on resync";
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
            {  1,   0,   1,    0,    0,   0,   0,   0,   1}, /* ing */
            {  1,   0,   0,    1,    0,   1,   0,   0,   1}, /* ized */
            {  1,   0,   0,    1,    1,   0,   0,   1,   1}, /* cted */
            {  1,   1,   0,    0,    0,   1,   0,   0,   1}, /* jer */
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

void wsrep::server_state::wait_until_state(
    wsrep::unique_lock<wsrep::mutex>& lock,
    enum wsrep::server_state::state state) const
{
    ++state_waiters_[state];
    while (state_ != state)
    {
        cond_.wait(lock);
        // If the waiter waits for any other state than disconnecting
        // or disconnected and the state has been changed to disconnecting,
        // this usually means that some error was encountered 
        if (state != s_disconnecting && state != s_disconnected
            && state_ == s_disconnecting)
        {
            throw wsrep::runtime_error("State wait was interrupted");
        }
    }
    --state_waiters_[state];
    cond_.notify_all();
}

void wsrep::server_state::interrupt_state_waiters(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED)
{
    assert(lock.owns_lock());
    cond_.notify_all();
}

void wsrep::server_state::close_orphaned_sr_transactions(
    wsrep::unique_lock<wsrep::mutex>& lock,
    wsrep::high_priority_service& high_priority_service)
{
    assert(lock.owns_lock());
    if (current_view_.own_index() == -1)
    {
        while (streaming_clients_.empty() == false)
        {
            streaming_clients_map::iterator i(streaming_clients_.begin());
            wsrep::client_id client_id(i->first);
            wsrep::transaction_id transaction_id(i->second->transaction().id());
            // It is safe to unlock the server state temporarily here.
            // The processing happens inside view handler which is
            // protected by the provider commit ordering critical
            // section. The lock must be unlocked temporarily to
            // allow converting the current client to streaming
            // applier in transaction::streaming_rollback().
            // The iterator i may be invalidated when the server state
            // remains unlocked, so it should not be accessed after
            // the bf abort call.
            lock.unlock();
            i->second->total_order_bf_abort(current_view_.view_seqno());
            lock.lock();
            streaming_clients_map::const_iterator found_i;
            while ((found_i = streaming_clients_.find(client_id)) !=
                   streaming_clients_.end() &&
                   found_i->second->transaction().id() == transaction_id)
            {
                cond_.wait(lock);
            }
        }
    }


    streaming_appliers_map::iterator i(streaming_appliers_.begin());
    while (i != streaming_appliers_.end())
    {
        if (std::find_if(current_view_.members().begin(),
                         current_view_.members().end(),
                         server_id_cmp(i->first.first)) ==
            current_view_.members().end())
        {
            wsrep::log_debug() << "Removing SR fragments for "
                               << i->first.first
                               << ", " << i->first.second;
            wsrep::id server_id(i->first.first);
            wsrep::transaction_id transaction_id(i->first.second);
            wsrep::high_priority_service* streaming_applier(i->second);
            high_priority_service.adopt_transaction(
                streaming_applier->transaction());
            {
                wsrep::high_priority_switch sw(high_priority_service,
                                               *streaming_applier);
                streaming_applier->rollback(
                    wsrep::ws_handle(), wsrep::ws_meta());
                streaming_applier->after_apply();
            }

            streaming_appliers_.erase(i++);
            server_service_.release_high_priority_service(streaming_applier);
            wsrep::ws_meta ws_meta(
                wsrep::gtid(),
                wsrep::stid(server_id, transaction_id, wsrep::client_id()),
                wsrep::seqno::undefined(), 0);
            lock.unlock();
            high_priority_service.remove_fragments(ws_meta);
            high_priority_service.commit(wsrep::ws_handle(transaction_id, 0),
                                         ws_meta);
            high_priority_service.after_apply();
            lock.lock();
        }
        else
        {
            ++i;
        }
    }
}

void wsrep::server_state::close_transactions_at_disconnect(
    wsrep::high_priority_service& high_priority_service)
{
    // Close streaming applier without removing fragments
    // from fragment storage. When the server is started again,
    // it must be able to recover ongoing streaming transactions.
    streaming_appliers_map::iterator i(streaming_appliers_.begin());
    while (i != streaming_appliers_.end())
    {
        wsrep::high_priority_service* streaming_applier(i->second);
        {
            wsrep::high_priority_switch sw(high_priority_service,
                                           *streaming_applier);
            streaming_applier->rollback(
                wsrep::ws_handle(), wsrep::ws_meta());
            streaming_applier->after_apply();
        }
        streaming_appliers_.erase(i++);
        server_service_.release_high_priority_service(streaming_applier);
    }
}
