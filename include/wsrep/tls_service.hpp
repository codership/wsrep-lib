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


/** @file tls_service.hpp
 *
 * Service interface for interacting with DBMS provided
 * TLS and encryption facilities.
 */

#ifndef WSREP_TLS_SERVICE_HPP
#define WSREP_TLS_SERVICE_HPP

#include "compiler.hpp"

#include <sys/types.h> // ssize_t

namespace wsrep
{

    /* Type tags for TLS context and TLS stream. */
    struct tls_context { };
    struct tls_stream { };

    /** @class tls_service
     *
     * TLS service interface. This provides an interface corresponding
     * to wsrep-API TLS service. For details see wsrep-API/wsrep_tls_service.h
     */
    class tls_service
    {
    public:
        enum status
        {
            success = 0,
            want_read,
            want_write,
            eof,
            error
        };

        struct op_result
        {
            /** Status code of the operation of negative system error number. */
            ssize_t status;
            /** Bytes transferred from/to given buffer during the operation. */
            size_t bytes_transferred;
        };

        virtual ~tls_service() { }
        /**
         * @return Zero on success, system error code on failure.
         */
        virtual tls_stream* create_tls_stream(int fd) WSREP_NOEXCEPT = 0;
        virtual void destroy(tls_stream*) WSREP_NOEXCEPT = 0;

        virtual int get_error_number(const tls_stream*) const WSREP_NOEXCEPT = 0;
        virtual const void* get_error_category(const tls_stream*) const WSREP_NOEXCEPT = 0;
        virtual const char* get_error_message(const tls_stream*,
                                              int value, const void* category)
            const WSREP_NOEXCEPT = 0;
        /**
         * @return Status enum.
         */
        virtual status client_handshake(tls_stream*) WSREP_NOEXCEPT = 0;

        /**
         * @return Status enum or negative error code.
         */
        virtual status server_handshake(tls_stream*) WSREP_NOEXCEPT = 0;

        /**
         * Read at most max_count bytes into buf.
         */
        virtual op_result read(tls_stream*,
                               void* buf, size_t max_count) WSREP_NOEXCEPT = 0;

        /**
         * Write at most count bytes from buf.
         */
        virtual op_result write(tls_stream*,
                                const void* buf, size_t count) WSREP_NOEXCEPT = 0;

        /**
         * Shutdown TLS stream.
         */
        virtual status shutdown(tls_stream*) WSREP_NOEXCEPT = 0;
    };
}

#endif // WSREP_TLS_SERVICE_HPP
