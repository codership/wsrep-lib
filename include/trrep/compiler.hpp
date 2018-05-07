//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//


/*! \file compiler.hpp
 *
 * Compiler specific options.
 */

#define TRREP_UNUSED __attribute__((unused))
#if __cplusplus >= 201103L
#define TRREP_OVERRIDE override
#else
#define TRREP_OVERRIDE
#endif // __cplusplus >= 201103L
