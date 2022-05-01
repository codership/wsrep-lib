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

/** @file operation_context.hpp
 *
 * A slightly more type safe way compared to void pointers to
 * pass operation contexts through wsrep-lib to callbacks.
 */

#ifndef WSREP_OPERATION_CONTEXT_HPP
#define WSREP_OPERATION_CONTEXT_HPP

namespace wsrep
{
    struct operation_context
    {
        virtual ~operation_context() = default;
    };

    struct null_operation_context : operation_context
    {
    };
} // namespace wsrep

#endif /* WSREP_OPERATION_CONTEXT_HPP */
