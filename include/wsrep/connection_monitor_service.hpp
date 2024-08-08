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


/** @file connection_monitor_service.hpp
 *
 * Service interface for interacting with DBMS provided
 * connection monitor callback.
 */

#ifndef WSREP_CONNECTION_MONITOR_SERVICE_HPP
#define WSREP_CONNECTION_MONITOR_SERVICE_HPP

#include "compiler.hpp"
#include "wsrep/buffer.hpp"

/* Type tag for connection key. */
typedef const void* wsrep_connection_key_t;

namespace wsrep
{
    class connection_monitor_service
    {
    public:

        virtual ~connection_monitor_service() { }

        /**
         * Connection monitor connect callback.
         */
        virtual bool connection_monitor_connect_cb(
                                                   wsrep_connection_key_t id,
                                                   const wsrep::const_buffer& scheme,
                                                   const wsrep::const_buffer& local_addr,
                                                   const wsrep::const_buffer& remote_addr
                                                  ) WSREP_NOEXCEPT = 0;
        /**
         * Connection monitor disconnect callback.
         */
        virtual bool connection_monitor_disconnect_cb(wsrep_connection_key_t id
                                                     ) WSREP_NOEXCEPT=0;

        /**
         * Connection monitor SSL/TLS info callback.
         */
        virtual bool connection_monitor_ssl_info_cb(wsrep_connection_key_t id,
                                                    const wsrep::const_buffer& ciper,
                                                    const wsrep::const_buffer& subject,
                                                    const wsrep::const_buffer& issuer,
                                                    const wsrep::const_buffer& version
                                                   ) WSREP_NOEXCEPT = 0;
    };
}

#endif // WSREP_CONNECTION_MONITOR_SERVICE_HPP
