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


/** @file allowlist_service.hpp
 *
 * Service interface for interacting with DBMS provided
 * allowlist callback.
 */

#ifndef WSREP_ALLOWLIST_SERVICE_HPP
#define WSREP_ALLOWLIST_SERVICE_HPP

#include "compiler.hpp"
#include "wsrep/buffer.hpp"

#include <sys/types.h> // ssize_t

namespace wsrep
{
    class allowlist_service
    {
    public:
        enum allowlist_key
        {
            /** IP allowlist check */
            allowlist_ip,
            /** SSL allowlist check */
            allowlist_ssl
        };

        virtual ~allowlist_service() { }

        /**
         * Allowlist callback.
         */
        virtual bool allowlist_cb(allowlist_key key,
                                  const wsrep::const_buffer& value) WSREP_NOEXCEPT = 0;
    };
}

#endif // WSREP_ALLOWLIST_SERVICE_HPP
