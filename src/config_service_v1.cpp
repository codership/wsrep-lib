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
#include "service_helpers.hpp"
#include "v26/wsrep_config_service.h"
#include "wsrep/logger.hpp"
#include "wsrep/provider_options.hpp"

namespace wsrep_config_service_v1
{
    wsrep_config_service_v1_t service{ 0 };

    int map_flags(int flags)
    {
        int option_flags = 0;
        if (flags & WSREP_PARAM_DEPRECATED)
            option_flags |= wsrep::provider_options::flag::deprecated;
        if (flags & WSREP_PARAM_READONLY)
            option_flags |= wsrep::provider_options::flag::readonly;
        if (flags & WSREP_PARAM_TYPE_BOOL)
            option_flags |= wsrep::provider_options::flag::type_bool;
        if (flags & WSREP_PARAM_TYPE_INTEGER)
            option_flags |= wsrep::provider_options::flag::type_integer;
        if (flags & WSREP_PARAM_TYPE_DOUBLE)
            option_flags |= wsrep::provider_options::flag::type_double;
        return option_flags;
    }

    void service_callback(const wsrep_parameter* p, void* context)
    {
        const int flags = map_flags(p->flags);
        std::unique_ptr<wsrep::provider_options::option_value> value;
        std::unique_ptr<wsrep::provider_options::option_value> default_value;
        wsrep::provider_options* options = (wsrep::provider_options*)context;
        switch (p->flags & WSREP_PARAM_TYPE_MASK)
        {
        case WSREP_PARAM_TYPE_BOOL:
            value.reset(new wsrep::provider_options::option_value_bool(
                p->value.as_bool));
            default_value.reset(new wsrep::provider_options::option_value_bool(
                p->value.as_bool));
            break;
        case WSREP_PARAM_TYPE_INTEGER:
            value.reset(new wsrep::provider_options::option_value_int(
                p->value.as_integer));
            default_value.reset(new wsrep::provider_options::option_value_int(
                p->value.as_integer));
            break;
        case WSREP_PARAM_TYPE_DOUBLE:
            value.reset(new wsrep::provider_options::option_value_double(
                p->value.as_double));
            default_value.reset(
                new wsrep::provider_options::option_value_double(
                    p->value.as_double));
            break;
        default:
            assert((p->flags & WSREP_PARAM_TYPE_MASK) == 0);
            const std::string str_value(p->value.as_string);
            value.reset(
                new wsrep::provider_options::option_value_string(str_value));
            default_value.reset(
                new wsrep::provider_options::option_value_string(str_value));
        }
        options->set_default(p->name, std::move(value),
                             std::move(default_value), flags);
    }
} // namespace wsrep_config_service_v1

static int config_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_config_service_v1_t*);
    typedef void (*deinit_fn)();
    return wsrep_impl::service_probe<init_fn>(
               dlh, WSREP_CONFIG_SERVICE_INIT_FUNC_V1, "config service v1")
           || wsrep_impl::service_probe<deinit_fn>(
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
    if (config_service_v1_probe(wsrep->dlh))
    {
        wsrep::log_warning() << "Provider does not support config service v1";
        return 1;
    }
    if (config_service_v1_init(wsrep->dlh))
    {
        wsrep::log_warning() << "Failed to initialize config service v1";
        return 1;
    }
    wsrep_config_service_v1::service.get_parameters(
        wsrep, &wsrep_config_service_v1::service_callback, options);
    config_service_v1_deinit(wsrep->dlh);
    return 0;
}
