/*
 * Copyright (C) 2024-2025 Codership Oy <info@codership.com>
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

#include "connection_monitor_service_v1.hpp"
#include "service_helpers.hpp"

#include "wsrep/buffer.hpp"
#include "v26/wsrep_connection_monitor_service.h"
#include "wsrep/connection_monitor_service.hpp"

#include <cassert>
#include <dlfcn.h>
#include <cerrno>

namespace wsrep_connection_monitor_service_v1
{
    // Pointer to connection monitor service implementation provided by
    // the application.
    static wsrep::connection_monitor_service* connection_monitor_service_impl{ 0 };
    static std::atomic<size_t> use_count;

    //
    // connection monitor service callbacks
    //

    void connection_monitor_connect_cb(
        wsrep_connection_monitor_context_t*,
        wsrep_connection_key_t id,
        const wsrep_buf_t* scheme,
        const wsrep_buf_t* local_address,
        const wsrep_buf_t* remote_address
    )
    {
        assert(connection_monitor_service_impl);
        wsrep::const_buffer scheme_value(scheme->ptr, scheme->len);
        wsrep::const_buffer remote_addr(remote_address->ptr, remote_address->len);
        wsrep::const_buffer local_addr(local_address->ptr, local_address->len);
        connection_monitor_service_impl->connection_monitor_connect_cb(
                                                               id,
                                                               scheme_value,
                                                               local_addr,
                                                               remote_addr);
    }

    void connection_monitor_disconnect_cb(
        wsrep_connection_monitor_context_t*,
        wsrep_connection_key_t id
    )
    {
        assert(connection_monitor_service_impl);
        connection_monitor_service_impl->connection_monitor_disconnect_cb(id);
    }

  void connection_monitor_ssl_info_cb(
        wsrep_connection_monitor_context_t*,
        wsrep_connection_key_t id,
        const wsrep_buf_t* cipher,
        const wsrep_buf_t* certificate_subject,
        const wsrep_buf_t* certificate_issuer,
        const wsrep_buf_t* version)
    {
        assert(connection_monitor_service_impl);
        wsrep::const_buffer ssl_cipher(cipher->ptr, cipher->len);
        wsrep::const_buffer cert_sub(certificate_subject->ptr, certificate_subject->len);
        wsrep::const_buffer cert_iss(certificate_issuer->ptr, certificate_issuer->len);
        wsrep::const_buffer vers(version->ptr, version->len);
        connection_monitor_service_impl->connection_monitor_ssl_info_cb(
            id, ssl_cipher, cert_sub, cert_iss, vers);
    }

    static wsrep_connection_monitor_service_v1_t connection_monitor_service_callbacks
        = { connection_monitor_connect_cb,
            connection_monitor_disconnect_cb,
	    connection_monitor_ssl_info_cb,
            0 };
}

int wsrep::connection_monitor_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_connection_monitor_service_v1_t*);
    typedef void (*deinit_fn)();
    if (wsrep_impl::service_probe<init_fn>(
            dlh, WSREP_CONNECTION_MONITOR_SERVICE_INIT_FUNC_V1, "connection monitor service v1") ||
        wsrep_impl::service_probe<deinit_fn>(
            dlh, WSREP_CONNECTION_MONITOR_SERVICE_DEINIT_FUNC_V1, "connection monitor service v1"))
    {
        return 1;
    }

    return 0;
}

int wsrep::connection_monitor_service_v1_init(void* dlh,
                                              wsrep::connection_monitor_service* connection_service)
{
    if (not (dlh && connection_service)) return EINVAL;
    typedef int (*init_fn)(wsrep_connection_monitor_service_v1_t*);
    wsrep_connection_monitor_service_v1::connection_monitor_service_impl = connection_service;
    int ret(0);
    if ((ret = wsrep_impl::service_init<init_fn>(
             dlh, WSREP_CONNECTION_MONITOR_SERVICE_INIT_FUNC_V1,
             &wsrep_connection_monitor_service_v1::connection_monitor_service_callbacks,
             "connection monitor service v1")))
    {
        wsrep_connection_monitor_service_v1::connection_monitor_service_impl = 0;
    }
    else
    {
        ++wsrep_connection_monitor_service_v1::use_count;
    }
    return ret;
}

void wsrep::connection_monitor_service_v1_deinit(void* dlh)
{
    typedef int (*deinit_fn)();
    wsrep_impl::service_deinit<deinit_fn>(
        dlh, WSREP_CONNECTION_MONITOR_SERVICE_DEINIT_FUNC_V1, "connection monitor service v1");
    assert(wsrep_connection_monitor_service_v1::use_count > 0);
    --wsrep_connection_monitor_service_v1::use_count;
    if (wsrep_connection_monitor_service_v1::use_count == 0)
    {
        wsrep_connection_monitor_service_v1::connection_monitor_service_impl = 0;
    }
}
