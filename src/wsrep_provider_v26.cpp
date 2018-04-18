//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep_provider_v26.hpp"
#include "exception.hpp"

#include <wsrep_api.h>

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
