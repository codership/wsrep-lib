/*
 * Copyright (C) 2019 Codership Oy <info@codership.com>
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

/** @file assert.hpp
 *
 * Assert macro to be used in header files which may be included
 * by the application. The macro WSREP_ASSERT() is independent of
 * definition of NDEBUG and is effective only with debug builds.
 *
 * In order to enable WSREP_ASSERT() define WSREP_LIB_HAVE_DEBUG.
 *
 * The reason for this macro is to make assertions in header files
 * independent of application preprocessor options.
 */

#ifndef WSREP_ASSERT_HPP
#define WSREP_ASSERT_HPP

#if defined(WSREP_LIB_HAVE_DEBUG)
#include <cassert>
#define WSREP_ASSERT(expr) assert(expr)
#else
#define WSREP_ASSERT(expr)
#endif // WSREP_LIB_HAVE_DEBUG

#endif // WSREP_ASSERT_HPP
