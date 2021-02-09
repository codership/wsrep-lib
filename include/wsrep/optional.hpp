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

/** @file optional.hpp
 *
 * Optional type similar to C++17 optional.
 */

#ifndef WSREP_OPTIONAL_HPP
#define WSREP_OPTIONAL_HPP

namespace wsrep
{
    template <class Type>
    class optional
    {
    public:
        /**
         * Default constructor. The value is default initialized
         * and the object is marked not to have a value.
         */
        optional()
            : value_{}
            , has_value_{}
        {
        }
        /**
         * Construct the option with a value.
         */
        optional(const Type& value)
            : value_{ value }
            , has_value_{ true }
        {
        }

        /**
         * Check if the object contains a value.
         */
        bool has_value() const { return has_value_; }
        operator bool() const { return has_value_; }

        /**
         * Return the value.
         */
        const Type& value() const { return value_; }
    private:
        Type value_;
        bool has_value_;
    };
}

#endif // WSREP_OPTIONAL_HPP
