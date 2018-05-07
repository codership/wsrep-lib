//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep_provider_v26.hpp"
#include "trrep/exception.hpp"

#include <wsrep_api.h>

#include <cassert>

#include <iostream>
#include <sstream>

trrep::wsrep_provider_v26::wsrep_provider_v26(
    const char* path,
    struct wsrep_init_args* args)
    : wsrep_()
{
    if (wsrep_load(path, &wsrep_, 0))
    {
        throw trrep::runtime_error("Failed to load wsrep library");
    }
    if (wsrep_->init(wsrep_, args) != WSREP_OK)
    {
        throw trrep::runtime_error("Failed to initialize wsrep provider");
    }
}

trrep::wsrep_provider_v26::~wsrep_provider_v26()
{
    wsrep_unload(wsrep_);
}

int trrep::wsrep_provider_v26::connect(
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

int trrep::wsrep_provider_v26::disconnect()
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

wsrep_status_t trrep::wsrep_provider_v26::run_applier(void *applier_ctx)
{
    return wsrep_->recv(wsrep_, applier_ctx);
}

int trrep::wsrep_provider_v26::append_key(wsrep_ws_handle_t* wsh,
                                          const wsrep_key_t* key)
{
    return (wsrep_->append_key(wsrep_, wsh, key, 1, WSREP_KEY_EXCLUSIVE, true)
            != WSREP_OK);
}

int trrep::wsrep_provider_v26::append_data(wsrep_ws_handle_t* wsh,
                                           const wsrep_buf_t* data)
{
    return (wsrep_->append_data(wsrep_, wsh, data, 1, WSREP_DATA_ORDERED, true)
            != WSREP_OK);
}

wsrep_status_t trrep::wsrep_provider_v26::certify(wsrep_conn_id_t conn_id,
                                                  wsrep_ws_handle_t* wsh,
                                                  uint32_t flags,
                                                  wsrep_trx_meta_t* meta)
{
    return wsrep_->certify(wsrep_, conn_id, wsh, flags, meta);
}

wsrep_status_t trrep::wsrep_provider_v26::bf_abort(
    wsrep_seqno_t bf_seqno,
    wsrep_trx_id_t victim_id,
    wsrep_seqno_t *victim_seqno)
{
    return wsrep_->abort_certification(
        wsrep_, bf_seqno, victim_id, victim_seqno);
}

wsrep_status_t trrep::wsrep_provider_v26::commit_order_enter(
    const wsrep_ws_handle_t* wsh,
    const wsrep_trx_meta_t* meta)
{
    return wsrep_->commit_order_enter(wsrep_, wsh, meta);
}

int trrep::wsrep_provider_v26::commit_order_leave(
    const wsrep_ws_handle_t* wsh,
    const wsrep_trx_meta_t* meta)
{
    return (wsrep_->commit_order_leave(wsrep_, wsh, meta, 0) != WSREP_OK);
}

int trrep::wsrep_provider_v26::release(wsrep_ws_handle_t* wsh)
{
    return (wsrep_->release(wsrep_, wsh) != WSREP_OK);
}

int trrep::wsrep_provider_v26::replay(wsrep_ws_handle_t* wsh,
                                      void* applier_ctx)
{
    return (wsrep_->replay_trx(wsrep_, wsh, applier_ctx) != WSREP_OK);
}

int trrep::wsrep_provider_v26::sst_sent(const wsrep_gtid_t& gtid, int err)
{
    if (wsrep_->sst_sent(wsrep_, &gtid, err) != WSREP_OK)
    {
        return 1;
    }
    return 0;
}

int trrep::wsrep_provider_v26::sst_received(const wsrep_gtid_t& gtid, int err)
{
    if (wsrep_->sst_received(wsrep_, &gtid, 0, err) != WSREP_OK)
    {
        return 1;
    }
    return 0;
}

std::vector<trrep::provider::status_variable>
trrep::wsrep_provider_v26::status() const
{
    std::vector<status_variable> ret;
    struct wsrep_stats_var* const stats(wsrep_->stats_get(wsrep_));
    struct wsrep_stats_var* i(stats);
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
