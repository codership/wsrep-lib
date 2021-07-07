/*
 * Copyright (C) 2020 Codership Oy <info@codership.com>
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

#include "ps_service_v1.hpp"

#include "wsrep/ps_service.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/provider.hpp"
#include "service_helpers.hpp"

#include <cassert>

namespace wsrep_ps_service_v1
{
    static wsrep::ps_service* ps_service_impl{0};
    static std::atomic<size_t> use_count;

    static wsrep_ps_service_v1_t ps_callbacks;
}

int wsrep::ps_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_ps_service_v1_t*);
    typedef void (*deinit_fn)();
    if (wsrep_impl::service_probe<init_fn>(
            dlh, WSREP_PS_SERVICE_INIT_FUNC_V1, "ps service v1") ||
        wsrep_impl::service_probe<deinit_fn>(
            dlh, WSREP_PS_SERVICE_DEINIT_FUNC_V1, "ps service v1"))
    {
        wsrep::log_warning() << "Provider does not support PS service v1";
        return 1;
    }
    return 0;
}

int wsrep::ps_service_v1_init(void* dlh,
                              wsrep::ps_service* ps_service)
{
    if (! (dlh && ps_service)) return EINVAL;

    typedef int (*init_fn)(wsrep_ps_service_v1_t*);
    wsrep_ps_service_v1::ps_service_impl = ps_service;
    int ret(0);
    if ((ret = wsrep_impl::service_init<init_fn>(
             dlh, WSREP_PS_SERVICE_INIT_FUNC_V1,
             &wsrep_ps_service_v1::ps_callbacks,
             "PS service v1")))
    {
        wsrep_ps_service_v1::ps_service_impl = 0;
    }
    else
    {
        ++wsrep_ps_service_v1::use_count;
    }
    return ret;
}

void wsrep::ps_service_v1_deinit(void* dlh)
{
    typedef int (*deinit_fn)();
    wsrep_impl::service_deinit<deinit_fn>(
        dlh, WSREP_PS_SERVICE_DEINIT_FUNC_V1, "ps service v1");
    --wsrep_ps_service_v1::use_count;
    if (wsrep_ps_service_v1::use_count == 0)
    {
        wsrep_ps_service_v1::ps_service_impl = 0;
    }
}

namespace wsrep
{

/**
 * Fetch cluster information to populate cluster members table.
 */
wsrep_status_t
    ps_service::fetch_pfs_info(provider* provider,
                               wsrep_node_info_t* nodes,
                               uint32_t* size) WSREP_NOEXCEPT
{
    wsrep_t* wsrep_ = reinterpret_cast<wsrep_t*>(provider->native());
    return wsrep_ps_service_v1::
        ps_callbacks.fetch_cluster_info(wsrep_, nodes, size);
}

/**
 * Fetch node information to populate node statistics table.
 */
wsrep_status_t
    ps_service::fetch_pfs_stat(provider* provider,
                               wsrep_node_stat_t* node) WSREP_NOEXCEPT
{
    wsrep_t* wsrep_ = reinterpret_cast<wsrep_t*>(provider->native());
    return wsrep_ps_service_v1::
        ps_callbacks.fetch_node_stat(wsrep_, node);
}

} // namespace "wsrep"
