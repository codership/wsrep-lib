//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep_provider_v26.hpp"
#include "exception.hpp"

#include <wsrep_api.h>

#include <iostream>

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
