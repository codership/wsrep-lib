//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "server_context.hpp"
#include "client_context.hpp"
#include "transaction_context.hpp"
#include "view.hpp"

// Todo: refactor into provider factory
#include "mock_provider.hpp"
#include "wsrep_provider_v26.hpp"

#include <wsrep_api.h>

#include <cassert>

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
        trrep::server_context& server_context(
            *reinterpret_cast<trrep::server_context*>(app_ctx));
        //
        // TODO: Fetch server id and group id from view infor
        //
        try
        {
            server_context.on_connect();
            return WSREP_CB_SUCCESS;
        }
        catch (const trrep::runtime_error& e)
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
        trrep::server_context& server_context(
            *reinterpret_cast<trrep::server_context*>(app_ctx));
        try
        {
            trrep::view view(*view_info);
            server_context.on_view(view);
            return WSREP_CB_SUCCESS;
        }
        catch (const trrep::runtime_error& e)
        {
            std::cerr << "Exception: " << e.what();
            return WSREP_CB_FAILURE;
        }
    }

    wsrep_cb_status_t sst_request_cb(void* app_ctx,
                                     void **sst_req, size_t* sst_req_len)
    {
        assert(app_ctx);
        trrep::server_context& server_context(
            *reinterpret_cast<trrep::server_context*>(app_ctx));

        try
        {
            std::string req(server_context.on_sst_request());
            *sst_req = ::strdup(req.c_str());
            *sst_req_len = strlen(req.c_str());
            return WSREP_CB_SUCCESS;
        }
        catch (const trrep::runtime_error& e)
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

        trrep::client_context* client_context(
            reinterpret_cast<trrep::client_context*>(ctx));
        assert(client_context);
        assert(client_context->mode() == trrep::client_context::m_applier);

        trrep::data data(buf->ptr, buf->len);
        if (client_context->start_transaction(*wsh, *meta, flags))
        {
            ret = WSREP_CB_FAILURE;
        }
        if (ret == WSREP_CB_SUCCESS &&
            client_context->server_context().on_apply(
                *client_context, client_context->transaction(), data))
        {
            ret = WSREP_CB_FAILURE;
        }
        return ret;
    }

    wsrep_cb_status_t synced_cb(void* app_ctx)
    {
        assert(app_ctx);
        trrep::server_context& server_context(
            *reinterpret_cast<trrep::server_context*>(app_ctx));
        try
        {
            server_context.on_sync();
            return WSREP_CB_SUCCESS;
        }
        catch (const trrep::runtime_error& e)
        {
            std::cerr << "On sync failed: " << e.what() << "\n";
            return WSREP_CB_FAILURE;
        }
    }


    wsrep_cb_status_t sst_donate_cb(void* app_ctx,
                                    void* ,
                                    const wsrep_buf_t* req_buf,
                                    const wsrep_gtid_t* gtid,
                                    const wsrep_buf_t*,
                                    bool bypass)
    {
        assert(app_ctx);
        trrep::server_context& server_context(
            *reinterpret_cast<trrep::server_context*>(app_ctx));
        try
        {
            std::string req(reinterpret_cast<const char*>(req_buf->ptr),
                            req_buf->len);
            server_context.on_sst_donate_request(req, *gtid, bypass);
            return WSREP_CB_SUCCESS;
        }
        catch (const trrep::runtime_error& e)
        {
            return WSREP_CB_FAILURE;
        }
    }
}

int trrep::server_context::load_provider(const std::string& provider_spec,
                                         const std::string& provider_options)
{
    std::cout << "Loading provider " << provider_spec << "\n";
    if (provider_spec == "mock")
    {
        provider_ = new trrep::mock_provider;
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
        provider_ = new trrep::wsrep_provider_v26(provider_spec.c_str(),
                                                  &init_args);
    }
    return 0;
}

trrep::server_context::~server_context()
{
    delete provider_;
}

void trrep::server_context::sst_received(const wsrep_gtid_t& gtid)
{
    provider_->sst_received(gtid, 0);
}

void trrep::server_context::wait_until_state(
    enum trrep::server_context::state state) const
{
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    while (state_ != state)
    {
        cond_.wait(lock);
    }
}

void trrep::server_context::on_connect()
{
    std::cout << "Server " << name_ << " connected to cluster" << "\n";
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    state_ = s_connected;
    cond_.notify_all();
}

void trrep::server_context::on_view(const trrep::view& view)
{
    std::cout << "================================================\nView:\n"
              << "id: " << view.id() << "\n"
              << "status: " << view.status() << "\n"
              << "own_index: " << view.own_index() << "\n"
              << "final: " << view.final() << "\n"
              << "members: \n";
    const std::vector<trrep::view::member>& members(view.members());
    for (std::vector<trrep::view::member>::const_iterator i(members.begin());
         i != members.end(); ++i)
    {
        std::cout << "id: " << i->id() << " "
                  << "name: " << i->name() << "\n";
    }
    std::cout << "=================================================\n";
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    if (view.final())
    {
        state_ = s_disconnected;
        cond_.notify_all();
    }
}

void trrep::server_context::on_sync()
{
    std::cout << "Synced with group" << "\n";
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    state_ = s_synced;
    cond_.notify_all();
}

int trrep::server_context::on_apply(
    trrep::client_context& client_context,
    trrep::transaction_context& transaction_context,
    const trrep::data& data)
{
    int ret(0);
    if (starts_transaction(transaction_context.flags()) &&
        commits_transaction(transaction_context.flags()))
    {
        assert(transaction_context.active() == false);
        transaction_context.start_transaction();
        if (client_context.apply(transaction_context, data))
        {
            ret = 1;
        }
        else if (client_context.commit(transaction_context))
        {
            ret = 1;
        }
        assert(ret ||
               transaction_context.state() ==
               trrep::transaction_context::s_committed);
    }
    else
    {
        // SR not implemented yet
        assert(0);
    }

    if (ret)
    {
        client_context.rollback(transaction_context);
    }

    transaction_context.after_statement();
    assert(transaction_context.active() == false);
    return ret;
}

bool trrep::server_context::statement_allowed_for_streaming(
    const trrep::client_context&,
    const trrep::transaction_context&) const
{
    /* Streaming not implemented yet. */
    return false;
}
