//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep_provider_v26.hpp"
#include "wsrep/exception.hpp"

#include <wsrep_api.h>

#include <cassert>

#include <iostream>
#include <sstream>

namespace
{
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
            /*! \todo Implement semi exclusive in API */
            assert(0);
            return WSREP_KEY_EXCLUSIVE;
        case wsrep::key::exclusive:     return WSREP_KEY_EXCLUSIVE;
        }
        throw wsrep::runtime_error("Invalid key type");
    }

    static inline wsrep_seqno_t seqno_to_native(wsrep::seqno seqno)
    {
        return (seqno.nil() ? WSREP_SEQNO_UNDEFINED : seqno.get());
    }

    static inline wsrep::seqno seqno_from_native(wsrep_seqno_t seqno)
    {
        return (seqno == WSREP_SEQNO_UNDEFINED ? 0 : seqno);
    }
    inline uint32_t map_one(const int flags, const int from,
                            const uint32_t to)
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

}

wsrep::wsrep_provider_v26::wsrep_provider_v26(
    const char* path,
    wsrep_init_args* args)
    : wsrep_()
{
    if (wsrep_load(path, &wsrep_, 0))
    {
        throw wsrep::runtime_error("Failed to load wsrep library");
    }
    if (wsrep_->init(wsrep_, args) != WSREP_OK)
    {
        throw wsrep::runtime_error("Failed to initialize wsrep provider");
    }
}

wsrep::wsrep_provider_v26::~wsrep_provider_v26()
{
    wsrep_unload(wsrep_);
}

int wsrep::wsrep_provider_v26::connect(
    const std::string& cluster_name,
    const std::string& cluster_url,
    const std::string& state_donor,
    bool bootstrap)
{
    int ret(0);
    wsrep_status_t wret;
    if ((wret = wsrep_->connect(wsrep_,
                                cluster_name.c_str(),
                                cluster_url.c_str(),
                                state_donor.c_str(),
                                bootstrap)) != WSREP_OK)
    {
        std::cerr << "Failed to connect cluster: "
                  << wret << "\n";
        ret = 1;
    }
    return ret;
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

int wsrep::wsrep_provider_v26::append_data(wsrep::ws_handle& ws_handle,
                                           const wsrep::data& data)
{
    const wsrep_buf_t wsrep_buf = {data.get().ptr(), data.get().size()};
    mutable_ws_handle mwsh(ws_handle);
    return (wsrep_->append_data(wsrep_, mwsh.native(), &wsrep_buf,
                                1, WSREP_DATA_ORDERED, true)
            != WSREP_OK);
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

int wsrep::wsrep_provider_v26::replay(wsrep::ws_handle& ws_handle,
                                      void* applier_ctx)
{
    mutable_ws_handle mwsh(ws_handle);
    return (wsrep_->replay_trx(wsrep_, mwsh.native(), applier_ctx) != WSREP_OK);
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
