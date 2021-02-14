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

/** @file galera_tls_context.hpp
 *
 * Tls_context implementation specific Galera provider, from versions
 * 4.x upwards.
 */

#ifndef WSREP_GALERA_TLS_CONTEXT_HPP
#define WSREP_GALERA_TLS_CONTEXT_HPP

#include "wsrep/tls_context.hpp"

#include <map>
#include <vector>

namespace wsrep
{
    class provider;
    class galera_tls_context : public tls_context
    {
    public:
        using tls_options_map_type = std::map<std::string, std::string>;
        galera_tls_context(wsrep::provider_options&);
        ~galera_tls_context();
        bool supports_tls() const override;
        bool is_enabled() const override;
        int enable() override;
        int reload() override;
        int configure(const conf&) override;
        conf get_configuration() override;
    private:
        provider_options& provider_options_;
        bool has_option(const std::string&) const;
        tls_options_map_type options_;
    };
}

#endif // WSREP_GALERA_TLS_CONTEXT_HPP
