//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep_provider_v26.hpp"

#include "wsrep/server_state.hpp"
#include "wsrep/client_state.hpp"
#include "wsrep/view.hpp"
#include "wsrep/exception.hpp"
#include "wsrep/logger.hpp"

#include <wsrep_api.h>

#include <cassert>

#include <iostream>
#include <sstream>

namespace
{
    /////////////////////////////////////////////////////////////////////
    //                           Helpers                               //
    /////////////////////////////////////////////////////////////////////

    enum wsrep::provider::status map_return_value(wsrep_status_t status)
    {
        switch (status)
        {
        case WSREP_OK:
            return wsrep::provider::success;
        case WSREP_WARNING:
            return wsrep::provider::error_warning;
        case WSREP_TRX_MISSING:
            return wsrep::provider::error_transaction_missing;
        case WSREP_TRX_FAIL:
            return wsrep::provider::error_certification_failed;
        case WSREP_BF_ABORT:
            return wsrep::provider::error_bf_abort;
        case WSREP_SIZE_EXCEEDED:
            return wsrep::provider::error_size_exceeded;
        case WSREP_CONN_FAIL:
            return wsrep::provider::error_connection_failed;
        case WSREP_NODE_FAIL:
            return wsrep::provider::error_provider_failed;
        case WSREP_FATAL:
            return wsrep::provider::error_fatal;
        case WSREP_NOT_IMPLEMENTED:
            return wsrep::provider::error_not_implemented;
        case WSREP_NOT_ALLOWED:
            return wsrep::provider::error_not_allowed;
        }
        return wsrep::provider::error_unknown;
    }

    wsrep_key_type_t map_key_type(enum wsrep::key::type type)
    {
        switch (type)
        {
        case wsrep::key::shared:        return WSREP_KEY_SHARED;
        case wsrep::key::semi_shared:   return WSREP_KEY_SEMI;
        case wsrep::key::semi_exclusive:
            /** @todo Implement semi exclusive in API */
            assert(0);
            return WSREP_KEY_EXCLUSIVE;
        case wsrep::key::exclusive:     return WSREP_KEY_EXCLUSIVE;
        }
        throw wsrep::runtime_error("Invalid key type");
    }

    static inline wsrep_seqno_t seqno_to_native(wsrep::seqno seqno)
    {
        return seqno.get();
    }

    static inline wsrep::seqno seqno_from_native(wsrep_seqno_t seqno)
    {
        return wsrep::seqno(seqno);
    }
    template <typename F, typename T>
    inline uint32_t map_one(const int flags, const F from,
                            const T to)
    {
        return ((flags & from) ? to : 0);
    }

    uint32_t map_flags_to_native(int flags)
    {
        using wsrep::provider;
        return (map_one(flags, provider::flag::start_transaction,
                        WSREP_FLAG_TRX_START) |
                map_one(flags, provider::flag::commit, WSREP_FLAG_TRX_END) |
                map_one(flags, provider::flag::rollback, WSREP_FLAG_ROLLBACK) |
                map_one(flags, provider::flag::isolation, WSREP_FLAG_ISOLATION) |
                map_one(flags, provider::flag::pa_unsafe, WSREP_FLAG_PA_UNSAFE) |
                // map_one(flags, provider::flag::commutative, WSREP_FLAG_COMMUTATIVE) |
                // map_one(flags, provider::flag::native, WSREP_FLAG_NATIVE) |
                map_one(flags, provider::flag::snapshot, WSREP_FLAG_SNAPSHOT));
    }

        int map_flags_from_native(uint32_t flags)
    {
        using wsrep::provider;
        return (map_one(flags,
                        WSREP_FLAG_TRX_START,
                        provider::flag::start_transaction) |
                map_one(flags,
                        WSREP_FLAG_TRX_END,
                        provider::flag::commit) |
                map_one(flags,
                        WSREP_FLAG_ROLLBACK,
                        provider::flag::rollback) |
                map_one(flags,
                        WSREP_FLAG_ISOLATION,
                        provider::flag::isolation) |
                map_one(flags,
                        WSREP_FLAG_PA_UNSAFE,
                        provider::flag::pa_unsafe) |
                // map_one(flags, provider::flag::commutative, WSREP_FLAG_COMMUTATIVE) |
                // map_one(flags, provider::flag::native, WSREP_FLAG_NATIVE) |
                map_one(flags, WSREP_FLAG_SNAPSHOT, provider::flag::snapshot));
    }

    class mutable_ws_handle
    {
    public:
        mutable_ws_handle(wsrep::ws_handle& ws_handle)
            : ws_handle_(ws_handle)
            , native_((wsrep_ws_handle_t)
                      {
                          ws_handle_.transaction_id().get(),
                          ws_handle_.opaque()
                      })
        { }

        ~mutable_ws_handle()
        {
            ws_handle_ = wsrep::ws_handle(native_.trx_id, native_.opaque);
        }

        wsrep_ws_handle_t* native()
        {
            return &native_;
        }
    private:
        wsrep::ws_handle& ws_handle_;
        wsrep_ws_handle_t native_;
    };

    class const_ws_handle
    {
    public:
        const_ws_handle(const wsrep::ws_handle& ws_handle)
            : ws_handle_(ws_handle)
            , native_((wsrep_ws_handle_t)
                      {
                          ws_handle_.transaction_id().get(),
                          ws_handle_.opaque()
                      })
        { }

        ~const_ws_handle()
        {
            assert(ws_handle_.transaction_id().get() == native_.trx_id);
            assert(ws_handle_.opaque() == native_.opaque);
        }

        const wsrep_ws_handle_t* native() const
        {
            return &native_;
        }
    private:
        const wsrep::ws_handle& ws_handle_;
        const wsrep_ws_handle_t native_;
    };

    class mutable_ws_meta
    {
    public:
        mutable_ws_meta(wsrep::ws_meta& ws_meta, int flags)
            : ws_meta_(ws_meta)
            , trx_meta_()
            , flags_(flags)
        { }

        ~mutable_ws_meta()
        {
            ws_meta_ = wsrep::ws_meta(
                wsrep::gtid(
                    wsrep::id(trx_meta_.gtid.uuid.data,
                              sizeof(trx_meta_.gtid.uuid.data)),
                    seqno_from_native(trx_meta_.gtid.seqno)),
                wsrep::stid(wsrep::id(trx_meta_.stid.node.data,
                                      sizeof(trx_meta_.stid.node.data)),
                            trx_meta_.stid.trx,
                            trx_meta_.stid.conn),
                seqno_from_native(trx_meta_.depends_on), flags_);
        }

        wsrep_trx_meta* native() { return &trx_meta_; }
        uint32_t native_flags() const { return map_flags_to_native(flags_); }
    private:
        wsrep::ws_meta& ws_meta_;
        wsrep_trx_meta_t trx_meta_;
        int flags_;
    };

    class const_ws_meta
    {
    public:
        const_ws_meta(const wsrep::ws_meta& ws_meta)
            : ws_meta_(ws_meta)
            , trx_meta_()
        {
            std::memcpy(trx_meta_.gtid.uuid.data, ws_meta.group_id().data(),
                        sizeof(trx_meta_.gtid.uuid.data));
            trx_meta_.gtid.seqno = seqno_to_native(ws_meta.seqno());
            std::memcpy(trx_meta_.stid.node.data, ws_meta.server_id().data(),
                        sizeof(trx_meta_.stid.node.data));
            trx_meta_.stid.conn = ws_meta.client_id().get();
            trx_meta_.stid.trx = ws_meta.transaction_id().get();
            trx_meta_.depends_on = seqno_to_native(ws_meta.depends_on());
        }

        ~const_ws_meta()
        {
        }

        const wsrep_trx_meta* native() const { return &trx_meta_; }
    private:
        const wsrep::ws_meta& ws_meta_;
        wsrep_trx_meta_t trx_meta_;
    };

    enum wsrep::view::status map_view_status_from_native(
        wsrep_view_status_t status)
    {
        switch (status)
        {
        case WSREP_VIEW_PRIMARY: return wsrep::view::primary;
        case WSREP_VIEW_NON_PRIMARY: return wsrep::view::non_primary;
        case WSREP_VIEW_DISCONNECTED: return wsrep::view::disconnected;
        default: throw wsrep::runtime_error("Unknown view status");
        }
    }

    /** @todo Currently capabilities defined in provider.hpp
     * are one to one with wsrep_api.h. However, the mapping should
     * be made explicit. */
    int map_capabilities_from_native(wsrep_cap_t capabilities)
    {
        return capabilities;
    }
    wsrep::view view_from_native(const wsrep_view_info& view_info)
    {
        std::vector<wsrep::view::member> members;
        for (int i(0); i < view_info.memb_num; ++i)
        {
            wsrep::id id(view_info.members[i].id.data, sizeof(view_info.members[i].id.data));
            std::string name(
                view_info.members[i].name,
                strnlen(view_info.members[i].name,
                        sizeof(view_info.members[i].name)));
            std::string incoming(
                view_info.members[i].incoming,
                strnlen(view_info.members[i].incoming,
                        sizeof(view_info.members[i].incoming)));
            members.push_back(wsrep::view::member(id, name, incoming));
        }
        return wsrep::view(
            wsrep::gtid(
                wsrep::id(view_info.state_id.uuid.data,
                          sizeof(view_info.state_id.uuid.data)),
                wsrep::seqno(view_info.state_id.seqno)),
            wsrep::seqno(view_info.view),
            map_view_status_from_native(view_info.status),
            map_capabilities_from_native(view_info.capabilities),
            view_info.my_idx,
            view_info.proto_ver,
            members);
    }

    /////////////////////////////////////////////////////////////////////
    //                         Callbacks                               //
    /////////////////////////////////////////////////////////////////////

    wsrep_cb_status_t connected_cb(
        void* app_ctx,
        const wsrep_view_info_t* view_info)
    {
        assert(app_ctx);
        wsrep::view view(view_from_native(*view_info));
        wsrep::server_state& server_state(
            *reinterpret_cast<wsrep::server_state*>(app_ctx));
        try
        {
            server_state.on_connect(view.state_id());
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
        wsrep::server_state& server_state(
            *reinterpret_cast<wsrep::server_state*>(app_ctx));
        try
        {
            wsrep::view view(view_from_native(*view_info));
            server_state.on_view(view);
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
        wsrep::server_state& server_state(
            *reinterpret_cast<wsrep::server_state*>(app_ctx));

        try
        {
            std::string req(server_state.prepare_for_sst());
            *sst_req = ::malloc(req.size() + 1);
            memcpy(*sst_req, req.data(), req.size() + 1);
            *sst_req_len = req.size() + 1;
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
        wsrep::client_state* client_state(
            reinterpret_cast<wsrep::client_state*>(ctx));
        assert(client_state);
        assert(client_state->mode() == wsrep::client_state::m_high_priority);

        wsrep::const_buffer data(buf->ptr, buf->len);
        wsrep::ws_handle ws_handle(wsh->trx_id, wsh->opaque);
        wsrep::ws_meta ws_meta(
            wsrep::gtid(wsrep::id(meta->gtid.uuid.data,
                                  sizeof(meta->gtid.uuid.data)),
                        wsrep::seqno(meta->gtid.seqno)),
            wsrep::stid(wsrep::id(meta->stid.node.data,
                                  sizeof(meta->stid.node.data)),
                        meta->stid.trx,
                        meta->stid.conn), wsrep::seqno(seqno_from_native(meta->depends_on)),
            map_flags_from_native(flags));
        try
        {
            if (client_state->server_state().on_apply(
                    *client_state, ws_handle, ws_meta, data))
            {
                return WSREP_CB_FAILURE;
            }
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            wsrep::log_error() << "Caught runtime error while applying "
                               << ws_meta.flags() << ": "
                               << e.what();
            return WSREP_CB_FAILURE;
        }
    }

    wsrep_cb_status_t synced_cb(void* app_ctx)
    {
        assert(app_ctx);
        wsrep::server_state& server_state(
            *reinterpret_cast<wsrep::server_state*>(app_ctx));
        try
        {
            server_state.on_sync();
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
        wsrep::server_state& server_state(
            *reinterpret_cast<wsrep::server_state*>(app_ctx));
        try
        {
            std::string req(reinterpret_cast<const char*>(req_buf->ptr),
                            req_buf->len);
            wsrep::gtid gtid(wsrep::id(req_gtid->uuid.data,
                                       sizeof(req_gtid->uuid.data)),
                             wsrep::seqno(req_gtid->seqno));
            if (server_state.start_sst(req, gtid, bypass))
            {
                return WSREP_CB_FAILURE;
            }
            return WSREP_CB_SUCCESS;
        }
        catch (const wsrep::runtime_error& e)
        {
            return WSREP_CB_FAILURE;
        }
    }

    void logger_cb(wsrep_log_level_t level, const char* msg)
    {
        switch (level)
        {
        case WSREP_LOG_FATAL:
        case WSREP_LOG_ERROR:
            wsrep::log_error() << "wsrep-lib: " << msg;
            break;
        case WSREP_LOG_WARN:
            wsrep::log_warning() << "wsrep-lib: " <<msg;
            break;
        case WSREP_LOG_INFO:
            wsrep::log_info() << "wsrep-lib: " << msg;
            break;
        case WSREP_LOG_DEBUG:
            wsrep::log_debug() << "wsrep-lib: " << msg;
            break;
        }
    }

}

wsrep::wsrep_provider_v26::wsrep_provider_v26(
    wsrep::server_state& server_state,
    const std::string& provider_options,
    const std::string& provider_spec)
    : provider(server_state)
    , wsrep_()
{
    struct wsrep_init_args init_args;
    memset(&init_args, 0, sizeof(init_args));
    init_args.app_ctx = &server_state;
    init_args.node_name = server_state_.name().c_str();
    init_args.node_address = server_state_.address().c_str();
    init_args.node_incoming = "";
    init_args.data_dir = server_state_.working_dir().c_str();
    init_args.options = provider_options.c_str();
    init_args.proto_ver = server_state.max_protocol_version();
    init_args.state_id = 0;
    init_args.state = 0;
    init_args.logger_cb = &logger_cb;
    init_args.connected_cb = &connected_cb;
    init_args.view_cb = &view_cb;
    init_args.sst_request_cb = &sst_request_cb;
    init_args.apply_cb = &apply_cb;
    init_args.unordered_cb = 0;
    init_args.sst_donate_cb = &sst_donate_cb;
    init_args.synced_cb = &synced_cb;

    if (wsrep_load(provider_spec.c_str(), &wsrep_, 0))
    {
        throw wsrep::runtime_error("Failed to load wsrep library");
    }
    if (wsrep_->init(wsrep_, &init_args) != WSREP_OK)
    {
        throw wsrep::runtime_error("Failed to initialize wsrep provider");
    }
}

wsrep::wsrep_provider_v26::~wsrep_provider_v26()
{
    wsrep_unload(wsrep_);
}

enum wsrep::provider::status wsrep::wsrep_provider_v26::connect(
    const std::string& cluster_name,
    const std::string& cluster_url,
    const std::string& state_donor,
    bool bootstrap)
{
    return map_return_value(wsrep_->connect(wsrep_,
                                            cluster_name.c_str(),
                                            cluster_url.c_str(),
                                            state_donor.c_str(),
                                            bootstrap));
}

int wsrep::wsrep_provider_v26::disconnect()
{
    int ret(0);
    wsrep_status_t wret;
    if ((wret = wsrep_->disconnect(wsrep_)) != WSREP_OK)
    {
        std::cerr << "Failed to disconnect from cluster: "
                  << wret << "\n";
        ret = 1;
    }
    return ret;
}

int wsrep::wsrep_provider_v26::capabilities() const
{
    return wsrep_->capabilities(wsrep_);
}
int wsrep::wsrep_provider_v26::desync()
{
    return (wsrep_->desync(wsrep_) != WSREP_OK);
}

int wsrep::wsrep_provider_v26::resync()
{
    return (wsrep_->resync(wsrep_) != WSREP_OK);
}

wsrep::seqno wsrep::wsrep_provider_v26::pause()
{
    return wsrep::seqno(wsrep_->pause(wsrep_));
}

int wsrep::wsrep_provider_v26::resume()
{
    return (wsrep_->resume(wsrep_) != WSREP_OK);
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::run_applier(void *applier_ctx)
{
    return map_return_value(wsrep_->recv(wsrep_, applier_ctx));
}

int wsrep::wsrep_provider_v26::append_key(wsrep::ws_handle& ws_handle,
                                          const wsrep::key& key)
{
    if (key.size() > 3)
    {
        assert(0);
        return 1;
    }
    wsrep_buf_t key_parts[3];
    for (size_t i(0); i < key.size(); ++i)
    {
        key_parts[i].ptr = key.key_parts()[i].ptr();
        key_parts[i].len = key.key_parts()[i].size();
    }
    wsrep_key_t wsrep_key = {key_parts, key.size()};
    mutable_ws_handle mwsh(ws_handle);
    return (wsrep_->append_key(
                wsrep_, mwsh.native(),
                &wsrep_key, 1, map_key_type(key.type()), true)
            != WSREP_OK);
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::append_data(wsrep::ws_handle& ws_handle,
                                           const wsrep::const_buffer& data)
{
    const wsrep_buf_t wsrep_buf = {data.data(), data.size()};
    mutable_ws_handle mwsh(ws_handle);
    return map_return_value(
        wsrep_->append_data(wsrep_, mwsh.native(), &wsrep_buf,
                            1, WSREP_DATA_ORDERED, true));
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::certify(wsrep::client_id client_id,
                                   wsrep::ws_handle& ws_handle,
                                   int flags,
                                   wsrep::ws_meta& ws_meta)
{
    mutable_ws_handle mwsh(ws_handle);
    mutable_ws_meta mmeta(ws_meta, flags);
    return map_return_value(
        wsrep_->certify(wsrep_, client_id.get(), mwsh.native(),
                        mmeta.native_flags(),
                        mmeta.native()));
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::bf_abort(
    wsrep::seqno bf_seqno,
    wsrep::transaction_id victim_id,
    wsrep::seqno& victim_seqno)
{
    wsrep_seqno_t wsrep_victim_seqno;
    wsrep_status_t ret(
        wsrep_->abort_certification(
            wsrep_, seqno_to_native(bf_seqno),
            victim_id.get(), &wsrep_victim_seqno));
    victim_seqno = seqno_from_native(wsrep_victim_seqno);
    return map_return_value(ret);
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::commit_order_enter(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    const_ws_handle cwsh(ws_handle);
    const_ws_meta cwsm(ws_meta);
    return map_return_value(
        wsrep_->commit_order_enter(wsrep_, cwsh.native(), cwsm.native()));
}

int wsrep::wsrep_provider_v26::commit_order_leave(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    const_ws_handle cwsh(ws_handle);
    const_ws_meta cwsm(ws_meta);
    return (wsrep_->commit_order_leave(wsrep_, cwsh.native(), cwsm.native(), 0) != WSREP_OK);
}

int wsrep::wsrep_provider_v26::release(wsrep::ws_handle& ws_handle)
{
    mutable_ws_handle mwsh(ws_handle);
    return (wsrep_->release(wsrep_, mwsh.native()) != WSREP_OK);
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::replay(const wsrep::ws_handle& ws_handle,
                                  void* applier_ctx)
{
    const_ws_handle mwsh(ws_handle);
    return map_return_value(
        wsrep_->replay_trx(wsrep_, mwsh.native(), applier_ctx));
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::enter_toi(
    wsrep::client_id client_id,
    const wsrep::key_array& keys,
    const wsrep::const_buffer& buffer,
    wsrep::ws_meta& ws_meta,
    int flags)
{
    mutable_ws_meta mmeta(ws_meta, flags);
    std::vector<std::vector<wsrep_buf_t> > key_parts;
    std::vector<wsrep_key_t> wsrep_keys;
    wsrep_buf_t wsrep_buf = {buffer.data(), buffer.size()};
    for (size_t i(0); i < keys.size(); ++i)
    {
        key_parts.push_back(std::vector<wsrep_buf_t>());
        for (size_t kp(0); kp < keys[i].size(); ++kp)
        {
            wsrep_buf_t buf = {keys[i].key_parts()[kp].data(),
                               keys[i].key_parts()[kp].size()};
            key_parts[i].push_back(buf);
        }
    }
    for (size_t i(0); i < key_parts.size(); ++i)
    {
        wsrep_key_t key = {key_parts[i].data(), key_parts[i].size()};
        wsrep_keys.push_back(key);
    }
    return map_return_value(wsrep_->to_execute_start(
                                wsrep_,
                                client_id.get(),
                                &wsrep_keys[0],
                                wsrep_keys.size(),
                                &wsrep_buf,
                                1,
                                mmeta.native_flags(),
                                mmeta.native()));
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::leave_toi(wsrep::client_id client_id)
{
    return map_return_value(wsrep_->to_execute_end(
                                wsrep_, client_id.get(), 0));
}

enum wsrep::provider::status
wsrep::wsrep_provider_v26::causal_read(int timeout) const
{
    return map_return_value(wsrep_->sync_wait(wsrep_, 0, timeout, 0));
}

int wsrep::wsrep_provider_v26::sst_sent(const wsrep::gtid& gtid, int err)
{
    wsrep_gtid_t wsrep_gtid;
    std::memcpy(wsrep_gtid.uuid.data, gtid.id().data(),
                sizeof(wsrep_gtid.uuid.data));
    wsrep_gtid.seqno = gtid.seqno().get();
    if (wsrep_->sst_sent(wsrep_, &wsrep_gtid, err) != WSREP_OK)
    {
        return 1;
    }
    return 0;
}

int wsrep::wsrep_provider_v26::sst_received(const wsrep::gtid& gtid, int err)
{
    wsrep_gtid_t wsrep_gtid;
    std::memcpy(wsrep_gtid.uuid.data, gtid.id().data(),
                sizeof(wsrep_gtid.uuid.data));
    wsrep_gtid.seqno = gtid.seqno().get();
    if (wsrep_->sst_received(wsrep_, &wsrep_gtid, 0, err) != WSREP_OK)
    {
        return 1;
    }
    return 0;
}

std::vector<wsrep::provider::status_variable>
wsrep::wsrep_provider_v26::status() const
{
    std::vector<status_variable> ret;
    wsrep_stats_var* const stats(wsrep_->stats_get(wsrep_));
    wsrep_stats_var* i(stats);
    if (i)
    {
        while (i->name)
        {
            switch (i->type)
            {
            case WSREP_VAR_STRING:
                ret.push_back(status_variable(i->name, i->value._string));
                break;
            case WSREP_VAR_INT64:
            {
                std::ostringstream os;
                os << i->value._int64;
                ret.push_back(status_variable(i->name, os.str()));
                break;
            }
            case WSREP_VAR_DOUBLE:
            {
                std::ostringstream os;
                os << i->value._double;
                ret.push_back(status_variable(i->name, os.str()));
                break;
            }
            default:
                assert(0);
                break;
            }
            ++i;
        }
        wsrep_->stats_free(wsrep_, stats);
    }
    return ret;
}

void wsrep::wsrep_provider_v26::reset_status()
{
    wsrep_->stats_reset(wsrep_);
}

std::string wsrep::wsrep_provider_v26::options() const
{
    std::string ret;
    char* opts;
    if ((opts = wsrep_->options_get(wsrep_)))
    {
        ret = opts;
        free(opts);
    }
    else
    {
        throw wsrep::runtime_error("Failed to get provider options");
    }
    return ret;
}

void wsrep::wsrep_provider_v26::options(const std::string& opts)
{
    if (wsrep_->options_set(wsrep_, opts.c_str()) != WSREP_OK)
    {
        throw wsrep::runtime_error("Failed to set provider options");
    }
}

void* wsrep::wsrep_provider_v26::native() const
{
    return wsrep_;
}
