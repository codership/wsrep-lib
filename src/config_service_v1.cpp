/*
 * Copyright (C) 2022 Codership Oy <info@codership.com>
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

#include "config_service_v1.hpp"
#include "v26/wsrep_config_service.h"
#include "service_helpers.hpp"
#include "wsrep/provider_options.hpp"

namespace wsrep_config_service_v1
{
    wsrep_config_service_v1_t service {0};
    void service_callback (const wsrep_parameter* p, void* context)
    {
        wsrep::provider_options* options = (wsrep::provider_options*)context;
        options->set_default(p->name, p->value);
    }
}

static int config_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_config_service_v1_t*);
    typedef void (*deinit_fn)();
    return wsrep_impl::service_probe<init_fn>(
      dlh, WSREP_CONFIG_SERVICE_INIT_FUNC_V1, "config service v1") ||
      wsrep_impl::service_probe<deinit_fn>(
        dlh, WSREP_CONFIG_SERVICE_DEINIT_FUNC_V1, "config service v1");
}

static int config_service_v1_init(void* dlh)
{
    typedef int (*init_fn)(wsrep_config_service_v1_t*);
    return wsrep_impl::service_init<init_fn>(
      dlh, WSREP_CONFIG_SERVICE_INIT_FUNC_V1,
        &wsrep_config_service_v1::service, "config service v1");
}

static void config_service_v1_deinit(void* dlh)
{
    typedef int (*deinit_fn)();
    wsrep_impl::service_deinit<deinit_fn>(
        dlh, WSREP_CONFIG_SERVICE_DEINIT_FUNC_V1, "config service v1");
}

int wsrep::config_service_v1_fetch(wsrep::provider& provider,
                                   wsrep::provider_options* options)
{
    struct wsrep_st* wsrep = (struct wsrep_st*)provider.native();
    if (config_service_v1_probe(wsrep->dlh)) {
      wsrep::log_warning() << "Provider does not support config service v1";
      return 1;
    }
    if (config_service_v1_init(wsrep->dlh)) {
      wsrep::log_warning() << "Failed to initialize config service v1";
      return 1;
    }
    wsrep_config_service_v1::service.get_parameters(
        wsrep, &wsrep_config_service_v1::service_callback, options);
    config_service_v1_deinit(wsrep->dlh);
    return 0;
}
