//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/server_context.hpp"
#include "wsrep/client_context.hpp"
#include "wsrep/transaction_context.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"

#include <cassert>
#include <sstream>

namespace
{

}

int wsrep::server_context::load_provider(const std::string& provider_spec,
                                         const std::string& provider_options)
{
    wsrep::log_info() << "Loading provider " << provider_spec;
    provider_ = wsrep::provider::make_provider(*this, provider_spec, provider_options);
    return (provider_ ? 0 : 1);
}

int wsrep::server_context::connect(const std::string& cluster_name,
                                   const std::string& cluster_address,
                                   const std::string& state_donor,
                                   bool bootstrap)
{
    return provider().connect(cluster_name, cluster_address, state_donor,
                              bootstrap);
}

int wsrep::server_context::disconnect()
{
    {
        wsrep::unique_lock<wsrep::mutex> lock(mutex_);
        state(lock, s_disconnecting);
    }
    return provider().disconnect();
}

wsrep::server_context::~server_context()
{
    delete provider_;
}

void wsrep::server_context::sst_sent(const wsrep::gtid& gtid, int error)
{
    provider_->sst_sent(gtid, error);
}
void wsrep::server_context::sst_received(const wsrep::gtid& gtid, int error)
{
    provider_->sst_received(gtid, error);
}

void wsrep::server_context::wait_until_state(
    enum wsrep::server_context::state state) const
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    ++state_waiters_[state];
    while (state_ != state)
    {
        cond_.wait(lock);
    }
    --state_waiters_[state];
    cond_.notify_all();
}

void wsrep::server_context::on_connect()
{
    wsrep::log() << "Server " << name_ << " connected to cluster";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    state(lock, s_connected);
}

void wsrep::server_context::on_view(const wsrep::view& view)
{
    wsrep::log() << "================================================\nView:\n"
                 << "id: " << view.id() << "\n"
                 << "status: " << view.status() << "\n"
                 << "own_index: " << view.own_index() << "\n"
                 << "final: " << view.final() << "\n"
                 << "members";
    const std::vector<wsrep::view::member>& members(view.members());
    for (std::vector<wsrep::view::member>::const_iterator i(members.begin());
         i != members.end(); ++i)
    {
        wsrep::log() << "id: " << i->id() << " "
                     << "name: " << i->name();
    }
    wsrep::log() << "=================================================";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (view.final())
    {
        state(lock, s_disconnected);
    }
}

void wsrep::server_context::on_sync()
{
    wsrep::log() << "Server " << name_ << " synced with group";
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (state_ != s_synced)
    {
        state(lock, s_synced);
    }
}

int wsrep::server_context::on_apply(
    wsrep::client_context& client_context,
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    const wsrep::const_buffer& data)
{
    int ret(0);
    const wsrep::transaction_context& txc(client_context.transaction());
    // wsrep::log_debug() << "server_context::on apply flags: "
    //                  << flags_to_string(ws_meta.flags());
    assert(ws_handle.opaque());
    assert(ws_meta.flags());
    bool not_replaying(txc.state() !=
                       wsrep::transaction_context::s_replaying);

    if (starts_transaction(ws_meta.flags()) &&
        commits_transaction(ws_meta.flags()))
    {
        if (not_replaying)
        {
            client_context.before_command();
            client_context.before_statement();
            assert(txc.active() == false);
            client_context.start_transaction(ws_handle, ws_meta);
        }
        if (client_context.apply(data))
        {
            ret = 1;
        }
        else if (client_context.commit())
        {
            ret = 1;
        }

        if (ret)
        {
            client_context.rollback();
        }

        if (not_replaying)
        {
            client_context.after_statement();
            client_context.after_command_before_result();
            client_context.after_command_after_result();
        }
        assert(ret ||
               txc.state() == wsrep::transaction_context::s_committed);
    }
    else
    {
        // SR not implemented yet
        assert(0);
    }
    if (not_replaying)
    {
        assert(txc.active() == false);
    }
    return ret;
}

bool wsrep::server_context::statement_allowed_for_streaming(
    const wsrep::client_context&,
    const wsrep::transaction_context&) const
{
    /* Streaming not implemented yet. */
    return false;
}

// Private

void wsrep::server_context::state(
    wsrep::unique_lock<wsrep::mutex>& lock WSREP_UNUSED,
    enum wsrep::server_context::state state)
{
    assert(lock.owns_lock());
    static const char allowed[n_states_][n_states_] =
        {
            /* dis, ing, ized, cted, jer, jed, dor, sed, ding */
            {  0,   1,   0,    1,    0,   0,   0,   0,   0}, /* dis */
            {  0,   0,   1,    0,    0,   0,   0,   0,   0}, /* ing */
            {  0,   0,   0,    1,    0,   1,   0,   0,   0}, /* ized */
            {  0,   0,   0,    0,    1,   0,   0,   1,   0}, /* cted */
            {  0,   0,   0,    0,    0,   1,   0,   1,   0}, /* jer */
            {  0,   0,   0,    0,    0,   0,   0,   1,   1}, /* jed */
            {  0,   0,   0,    0,    0,   1,   0,   0,   1}, /* dor */
            {  0,   0,   0,    0,    0,   1,   1,   0,   1}, /* sed */
            {  1,   0,   0,    0,    0,   0,   0,   0,   0}  /* ding */
        };

    if (allowed[state_][state])
    {
        wsrep::log() << "server " << name_ << " state change: "
                     << state_ << " -> " << state;
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
