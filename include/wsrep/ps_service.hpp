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

/** @file ps_service.hpp
 *
 * Service interface for interacting with Performance Scheme
 * tables at DBMS side.
 */

#ifndef WSREP_PS_SERVICE_HPP
#define WSREP_PS_SERVICE_HPP

#include "compiler.hpp"
#include "provider.hpp"
#include "wsrep_ps.h"

namespace wsrep
{
    /** @class ps_service
     *
     * PS service interface. This provides an interface corresponding
     * to wsrep PS service. For details see wsrep-API/ps/wsrep_ps.h
     */
    class ps_service
    {
    public:
        virtual ~ps_service() { }
        /**
         * Fetch cluster information to populate cluster members table.
         */
        virtual wsrep_status_t
            fetch_pfs_info(provider* provider,
                           wsrep_node_info_t* nodes,
                           uint32_t* size) WSREP_NOEXCEPT = 0;
        /**
         * Fetch node information to populate node statistics table.
         */
        virtual wsrep_status_t
            fetch_pfs_stat(provider* provider,
                           wsrep_node_stat_t* node) WSREP_NOEXCEPT = 0;
    };
}

#endif // WSREP_PS_SERVICE_HPP
