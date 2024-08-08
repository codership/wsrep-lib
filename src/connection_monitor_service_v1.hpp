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

#ifndef WSREP_CONNECTION_MONITOR_SERVICE_V1_HPP
#define WSREP_CONNECTION_MONITOR_SERVICE_V1_HPP

namespace wsrep
{
    class connection_monitor_service;
    /**
     * Probe connection_monitor_service_v1 support in loaded library.
     *
     * @param dlh Handle returned by dlopen().
     *
     * @return Zero on success, non-zero system error code on failure.
     */
    int connection_monitor_service_v1_probe(void *dlh);

    /**
     * Initialize the connection_monitor service.
     *
     * @param dlh Handle returned by dlopen().
     * @param connection_monitor_service Pointer to
     *            wsrep::connection_monitor_service implementation.
     *
     * @return Zero on success, non-zero system error code on failure.
     */
    int connection_monitor_service_v1_init(void* dlh,
            wsrep::connection_monitor_service* connection_monitor_service);

    /**
     * Deinitialize the connection monitor service.
     *
     * @params dlh Handler returned by dlopen().
     */
    void connection_monitor_service_v1_deinit(void* dlh);

}

#endif // WSREP_CONNECTION_MONITOR_SERVICE_V1_HPP
