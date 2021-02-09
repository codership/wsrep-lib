/*
 * Copyright (C) 2022 Codership Oy <info@codership.com>
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

#ifndef WSREP_CONFIG_SERVICE_V1_HPP
#define WSREP_CONFIG_SERVICE_V1_HPP

namespace wsrep
{
    class provider;
    class provider_options;
    int config_service_v1_fetch(provider& provider, provider_options* opts);
} // namespace wsrep

#endif // WSREP_CONFIG_SERVICE_V1_HPP
