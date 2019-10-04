/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
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


/** @file compiler.hpp
 *
 * Compiler specific macro definitions.
 *
 * WSREP_OVERRIDE - Set to "override" if the compiler supports it, otherwise
 *                  left empty.
 * WSREP_CONSTEXPR_OR_INLINE - Set to "constexpr" if the compiler supports it,
 *                             otherwise "inline".
 */

#define WSREP_UNUSED __attribute__((unused))
#if __cplusplus >= 201103L
#define WSREP_OVERRIDE override
#define WSREP_CONSTEXPR_OR_INLINE constexpr
#else
#define WSREP_OVERRIDE
#define WSREP_CONSTEXPR_OR_INLINE inline
#endif // __cplusplus >= 201103L
