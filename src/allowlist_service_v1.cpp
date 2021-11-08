/*
 * Copyright (C) 2021 Codership Oy <info@codership.com>
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

#include "allowlist_service_v1.hpp"
#include "service_helpers.hpp"

#include "wsrep/allowlist_service.hpp"
#include "wsrep/buffer.hpp"
#include "v26/wsrep_allowlist_service.h"

#include <cassert>
#include <dlfcn.h>
#include <cerrno>

namespace wsrep_allowlist_service_v1
{
    // Pointer to allowlist service implementation provided by
    // the application.
    static wsrep::allowlist_service* allowlist_service_impl{ 0 };
    static std::atomic<size_t> use_count;

   enum wsrep::allowlist_service::allowlist_key allowlist_key_from_native(wsrep_allowlist_key_t key)
   {
        switch (key)
        {
        case WSREP_ALLOWLIST_KEY_IP: return wsrep::allowlist_service::allowlist_key::allowlist_ip;
        case WSREP_ALLOWLIST_KEY_SSL: return wsrep::allowlist_service::allowlist_key::allowlist_ssl;
        default: throw wsrep::runtime_error("Unknown allowlist key");
        }
   }

    //
    // allowlist service callbacks
    //

    wsrep_status_t allowlist_cb(
        wsrep_allowlist_context_t*,
        wsrep_allowlist_key_t key,
        const wsrep_buf_t* value
    )
    {
        assert(allowlist_service_impl);
        wsrep::const_buffer allowlist_value(value->ptr, value->len);
        if (allowlist_service_impl->allowlist_cb(allowlist_key_from_native(key),
                                                 allowlist_value))
        {
            return WSREP_OK;
        }
        return WSREP_NOT_ALLOWED;
    }

    static wsrep_allowlist_service_v1_t allowlist_service_callbacks
        = { allowlist_cb,
            0 };
}

int wsrep::allowlist_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_allowlist_service_v1_t*);
    typedef void (*deinit_fn)();
    if (wsrep_impl::service_probe<init_fn>(
            dlh, WSREP_ALLOWLIST_SERVICE_INIT_FUNC_V1, "allowlist service v1") ||
        wsrep_impl::service_probe<deinit_fn>(
            dlh, WSREP_ALLOWLIST_SERVICE_DEINIT_FUNC_V1, "allowlist service v1"))
    {
        wsrep::log_warning() << "Provider does not support allowlist service v1";
        return 1;
    }
    return 0;
}

int wsrep::allowlist_service_v1_init(void* dlh,
                                     wsrep::allowlist_service* allowlist_service)
{
    if (not (dlh && allowlist_service)) return EINVAL;
    typedef int (*init_fn)(wsrep_allowlist_service_v1_t*);
    wsrep_allowlist_service_v1::allowlist_service_impl = allowlist_service;
    int ret(0);
    if ((ret = wsrep_impl::service_init<init_fn>(
             dlh, WSREP_ALLOWLIST_SERVICE_INIT_FUNC_V1,
             &wsrep_allowlist_service_v1::allowlist_service_callbacks,
             "allowlist service v1")))
    {
        wsrep_allowlist_service_v1::allowlist_service_impl = 0;
    }
    else
    {
        ++wsrep_allowlist_service_v1::use_count;
    }
    return ret;
}

void wsrep::allowlist_service_v1_deinit(void* dlh)
{
    typedef int (*deinit_fn)();
    wsrep_impl::service_deinit<deinit_fn>(
        dlh, WSREP_ALLOWLIST_SERVICE_DEINIT_FUNC_V1, "allowlist service v1");
    --wsrep_allowlist_service_v1::use_count;
    if (wsrep_allowlist_service_v1::use_count == 0)
    {
        wsrep_allowlist_service_v1::allowlist_service_impl = 0;
    }
}
