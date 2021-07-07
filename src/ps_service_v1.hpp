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

#ifndef WSREP_PS_SERVICE_V1_HPP
#define WSREP_PS_SERVICE_V1_HPP

namespace wsrep
{
    class ps_service;
    /**
     * Probe thread_service_v1 support in loaded library.
     *
     * @param dlh Handle returned by dlopen().
     *
     * @return Zero on success, non-zero system error code on failure.
     */
    int ps_service_v1_probe(void *dlh);

    /**
     * Initialize PS service.
     *
     * @param dlh Handle returned by dlopen().
     * @params thread_service Pointer to wsrep::thread_service implementation.
     *
     * @return Zero on success, non-zero system error code on failure.
     */
    int ps_service_v1_init(void* dlh,
                           wsrep::ps_service* ps_service);

    /**
     * Deinitialize PS service.
     *
     * @param dlh Handler returned by dlopen().
     */
    void ps_service_v1_deinit(void* dlh);
}

#endif // WSREP_PS_SERVICE_V1_HPP
