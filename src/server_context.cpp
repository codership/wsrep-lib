//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/server_context.hpp"
#include "wsrep/client_context.hpp"
#include "wsrep/transaction_context.hpp"
#include "wsrep/view.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/compiler.hpp"

// Todo: refactor into provider factory
#include "mock_provider.hpp"
#include "wsrep_provider_v26.hpp"

#include <cassert>
#include <sstream>

namespace
{

    inline bool starts_transaction(uint32_t flags)
    {
        return (flags & WSREP_FLAG_TRX_START);
    }

    inline bool commits_transaction(uint32_t flags)
    {
        return (flags & WSREP_FLAG_TRX_END);
    }

    inline bool rolls_back_transaction(uint32_t flags)
    {
        return (flags & WSREP_FLAG_ROLLBACK);
    }

    wsrep_cb_status_t connected_cb(
        void* app_ctx,
        const wsrep_view_info_t* view __attribute((unused)))
    {
        assert(app_ctx);
        wsrep::server_context& server_context(
            *reinterpret_cast<wsrep::server_context*>(app_ctx));
        //
        // TODO: Fetch server id and group id from view infor
        //
        try
        {
            server_context.on_connect();
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            std::cerr << "Exception: " << e.what();
            return WSREP_CB_FAILURE;
        }
    }

    wsrep_cb_status_t view_cb(void* app_ctx,
                                     void* recv_ctx __attribute__((unused)),
                                     const wsrep_view_info_t* view_info,
                                     const char*,
                                     size_t)
    {
        assert(app_ctx);
        assert(view_info);
        wsrep::server_context& server_context(
            *reinterpret_cast<wsrep::server_context*>(app_ctx));
        try
        {
            wsrep::view view(*view_info);
            server_context.on_view(view);
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            std::cerr << "Exception: " << e.what();
            return WSREP_CB_FAILURE;
        }
    }

    wsrep_cb_status_t sst_request_cb(void* app_ctx,
                                     void **sst_req, size_t* sst_req_len)
    {
        assert(app_ctx);
        wsrep::server_context& server_context(
            *reinterpret_cast<wsrep::server_context*>(app_ctx));

        try
        {
            std::string req(server_context.on_sst_required());
            *sst_req = ::strdup(req.c_str());
            *sst_req_len = strlen(req.c_str());
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            return WSREP_CB_FAILURE;
        }
    }

    wsrep_cb_status_t apply_cb(void* ctx,
                               const wsrep_ws_handle_t* wsh,
                               uint32_t flags,
                               const wsrep_buf_t* buf,
                               const wsrep_trx_meta_t* meta,
                               wsrep_bool_t* exit_loop __attribute__((unused)))
    {
        wsrep_cb_status_t ret(WSREP_CB_SUCCESS);

        wsrep::client_context* client_context(
            reinterpret_cast<wsrep::client_context*>(ctx));
        assert(client_context);
        assert(client_context->mode() == wsrep::client_context::m_applier);

        wsrep::data data(buf->ptr, buf->len);
        wsrep::ws_handle ws_handle(wsh->trx_id, wsh->opaque);
        wsrep::ws_meta ws_meta(
            wsrep::gtid(wsrep::id(meta->gtid.uuid.data,
                                  sizeof(meta->gtid.uuid.data)),
                        meta->gtid.seqno),
            wsrep::stid(wsrep::id(meta->stid.node.data,
                                  sizeof(meta->stid.node.data)),
                        meta->stid.trx,
                        meta->stid.conn), meta->depends_on, flags);
        if (client_context->transaction().state() !=
            wsrep::transaction_context::s_replaying &&
            client_context->start_transaction(ws_handle, ws_meta))
        {
            ret = WSREP_CB_FAILURE;
        }
        if (ret == WSREP_CB_SUCCESS &&
            client_context->server_context().on_apply(
                *client_context, data))
        {
            ret = WSREP_CB_FAILURE;
        }
        return ret;
    }

    wsrep_cb_status_t synced_cb(void* app_ctx)
    {
        assert(app_ctx);
        wsrep::server_context& server_context(
            *reinterpret_cast<wsrep::server_context*>(app_ctx));
        try
        {
            server_context.on_sync();
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            std::cerr << "On sync failed: " << e.what() << "\n";
            return WSREP_CB_FAILURE;
        }
    }


    wsrep_cb_status_t sst_donate_cb(void* app_ctx,
                                    void* ,
                                    const wsrep_buf_t* req_buf,
                                    const wsrep_gtid_t* req_gtid,
                                    const wsrep_buf_t*,
                                    bool bypass)
    {
        assert(app_ctx);
        wsrep::server_context& server_context(
            *reinterpret_cast<wsrep::server_context*>(app_ctx));
        try
        {
            std::string req(reinterpret_cast<const char*>(req_buf->ptr),
                            req_buf->len);
            wsrep::gtid gtid(wsrep::id(req_gtid->uuid.data,
                                       sizeof(req_gtid->uuid.data)),
                             req_gtid->seqno);
            server_context.on_sst_request(req, gtid, bypass);
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            return WSREP_CB_FAILURE;
        }
    }
}

int wsrep::server_context::load_provider(const std::string& provider_spec,
                                         const std::string& provider_options)
{
    wsrep::log() << "Loading provider " << provider_spec;
    if (provider_spec == "mock")
    {
        provider_ = new wsrep::mock_provider;
    }
    else
    {
        std::cerr << "Provider options" << provider_options << "\n";
        struct wsrep_init_args init_args;
        memset(&init_args, 0, sizeof(init_args));
        init_args.app_ctx = this;
        init_args.node_name = name_.c_str();
        init_args.node_address = address_.c_str();
        init_args.node_incoming = "";
        init_args.data_dir = working_dir_.c_str();
        init_args.options = provider_options.c_str();
        init_args.proto_ver = 1;
        init_args.state_id = 0;
        init_args.state = 0;
        init_args.logger_cb = 0;
        init_args.connected_cb = &connected_cb;
        init_args.view_cb = &view_cb;
        init_args.sst_request_cb = &sst_request_cb;
        init_args.apply_cb = &apply_cb;
        init_args.unordered_cb = 0;
        init_args.sst_donate_cb = &sst_donate_cb;
        init_args.synced_cb = &synced_cb;

        std::cerr << init_args.options << "\n";
        provider_ = new wsrep::wsrep_provider_v26(provider_spec.c_str(),
                                                  &init_args);
    }
    return 0;
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
    const wsrep::data& data)
{
    int ret(0);
    const wsrep::transaction_context& txc(client_context.transaction());
    if (starts_transaction(txc.flags()) &&
        commits_transaction(txc.flags()))
    {
        bool not_replaying(txc.state() !=
                           wsrep::transaction_context::s_replaying);
        if (not_replaying)
        {
            client_context.before_command();
            client_context.before_statement();
            assert(txc.active() == false);
            client_context.start_transaction();
        }
        if (client_context.apply(data))
        {
            ret = 1;
        }
        else if (client_context.commit())
        {
            ret = 1;
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

    if (ret)
    {
        client_context.rollback();
    }

    client_context.after_statement();
    assert(txc.active() == false);
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
        std::cerr << os.str() << "\n";
        ::abort();
        // throw wsrep::runtime_error(os.str());
    }
}
