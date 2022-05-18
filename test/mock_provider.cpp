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

#include "mock_provider.hpp"

#include "../src/galera_tls_context.hpp"

std::unique_ptr<wsrep::provider_tls_context>
wsrep::mock_provider::make_tls_context(
    wsrep::provider_options& provider_options)
{
    return std::unique_ptr<wsrep::provider_tls_context>(
        new wsrep::galera_tls_context(provider_options));
}
