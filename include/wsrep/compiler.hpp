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
 * WSREP_NOEXCEPT - Specifies that the method/function does not throw. If
 *                  and exception is thrown inside, std::terminate is called
 *                  without propagating the exception.
 *                  Set to "noexcept" if the compiler supports it, otherwise
 *                  left empty.
 * WSREP_NORETURN - Indicates that the method/function does not return.
 *                  Set to attribute "[[noreturn]]" if the compiler supports,
 *                  it, otherwise "__attribute__((noreturn))".
 * WSREP_OVERRIDE - Set to "override" if the compiler supports it, otherwise
 *                  left empty.
 * WSREP_UNUSED - Can be used to mark variables which may be present in
 *                debug builds but not in release builds.
 * WSREP_FALLTHROUGH - Silence implicit fallthrough warning.
 */

#ifndef WSREP_LIB_COMPILER_HPP
#define WSREP_LIB_COMPILER_HPP

#if __cplusplus >= 201103L && !(__GNUC__ == 4 && __GNUG_MINOR__ < 8)
#define WSREP_NORETURN [[noreturn]]
#else
#define WSREP_NORETURN __attribute__((noreturn))
#endif // __cplusplus >= 201103L && !(__GNUC__ == 4 && __GNUG_MINOR__ < 8)

#if __cplusplus >= 201103L
#define WSREP_NOEXCEPT noexcept
#define WSREP_OVERRIDE override
#else
#define WSREP_NOEXCEPT
#define WSREP_OVERRIDE
#endif // __cplusplus >= 201103L
#define WSREP_UNUSED __attribute__((unused))

#if __GNUC__ >= 7
#define WSREP_FALLTHROUGH __attribute__((fallthrough))
#elif defined(__clang__)
#  if defined(__has_warning)
#    if __has_feature(cxx_attributes) && __has_warning("-Wimplicit-fallthrough")
#      define WSREP_FALLTHROUGH [[clang::fallthrough]]
#    endif
#  endif
#else  // __clang __
#define WSREP_FALLTHROUGH ((void)0)
#endif // __GNUC__ >= 7 || (__clang__ && __clang_major__ >= 10)

#endif // WSREP_LIB_COMPILER_HPP
