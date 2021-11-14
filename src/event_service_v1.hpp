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

#ifndef WSREP_EVENT_SERVICE_V1_HPP
#define WSREP_EVENT_SERVICE_V1_HPP

namespace wsrep
{
    class event_service;
    /**
     * Probe event_service_v1 support in loaded library.
     *
     * @param dlh Handle returned by dlopen().
     *
     * @return Zero on success, non-zero system error code on failure.
     */
    int event_service_v1_probe(void *dlh);

    /**
     * Initialize event service.
     *
     * @param dlh Handle returned by dlopen().
     * @params event_service Pointer to wsrep::event_service implementation.
     *
     * @return Zero on success, non-zero system error code on failure.
     */
    int event_service_v1_init(void* dlh,
                              wsrep::event_service* event_service);

    /**
     * Deinitialize event service.
     *
     * @param dlh Handler returned by dlopen().
     */
    void event_service_v1_deinit(void* dlh);
}

#endif // WSREP_EVENT_SERVICE_V1_HPP
