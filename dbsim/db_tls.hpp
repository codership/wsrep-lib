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

#ifndef WSREP_DB_TLS_HPP
#define WSREP_DB_TLS_HPP

#include "wsrep/tls_service.hpp"

#include <string>

namespace db
{
    class tls : public wsrep::tls_service
    {
    public:
        virtual wsrep::tls_stream* create_tls_stream(int)
            WSREP_NOEXCEPT override;
        virtual void destroy(wsrep::tls_stream*) WSREP_NOEXCEPT override;
        virtual int get_error_number(const wsrep::tls_stream*) const
            WSREP_NOEXCEPT override;
        virtual const void* get_error_category(const wsrep::tls_stream*) const
            WSREP_NOEXCEPT override;
        virtual const char* get_error_message(const wsrep::tls_stream*,
                                              int, const void*) const
            WSREP_NOEXCEPT override;
        virtual enum wsrep::tls_service::status
        client_handshake(wsrep::tls_stream*)
            WSREP_NOEXCEPT override;
        virtual enum wsrep::tls_service::status
        server_handshake(wsrep::tls_stream*)
            WSREP_NOEXCEPT override;
        virtual wsrep::tls_service::op_result
        read(wsrep::tls_stream*, void* buf, size_t max_count)
            WSREP_NOEXCEPT override;
        virtual wsrep::tls_service::op_result
        write(wsrep::tls_stream*, const void* buf, size_t count)
            WSREP_NOEXCEPT override;
        virtual wsrep::tls_service::status
        shutdown(wsrep::tls_stream*) WSREP_NOEXCEPT override;

        static void init(int mode);
        static std::string stats();
    };
};

#endif // WSREP_DB_TLS_HPP
