//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//


/** @file compiler.hpp
 *
 * Compiler specific options.
 */

#define WSREP_UNUSED __attribute__((unused))
#if __cplusplus >= 201103L
#define WSREP_OVERRIDE override
#else
#define WSREP_OVERRIDE
#endif // __cplusplus >= 201103L
