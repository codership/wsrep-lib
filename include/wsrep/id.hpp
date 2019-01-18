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

/** @file id.hpp
 *
 * A generic identifier utility class.
 */
#ifndef WSREP_ID_HPP
#define WSREP_ID_HPP

#include "exception.hpp"

#include <iosfwd>
#include <cstring> // std::memset()

namespace wsrep
{
    /**
     * The idientifier class stores identifiers either in UUID
     * format or in string format. The storage format is decided
     * upon construction. If the given string contains a valid
     * UUID, the storage format will be binary. Otherwise the
     * string will be copied verbatim. If the string format is used,
     * the maximum length of the identifier is limited to 16 bytes.
     */
    class id
    {
    public:
        /**
         * Default constructor. Constructs an empty identifier.
         */
        id() : data_() { std::memset(data_, 0, sizeof(data_)); }

        /**
         * Construct from string. The input string may contain either
         * valid UUID or a string with maximum 16 bytes length.
         */
        id(const std::string&);

        /**
         * Construct from void pointer.
         */
        id (const void* data, size_t size) : data_()
        {
            if (size > 16)
            {
                throw wsrep::runtime_error("Too long identifier");
            }
            std::memset(data_, 0, sizeof(data_));
            std::memcpy(data_, data, size);
        }

        bool operator<(const id& other) const
        {
            return (std::memcmp(data_, other.data_, sizeof(data_)) < 0);
        }

        bool operator==(const id& other) const
        {
            return (std::memcmp(data_, other.data_, sizeof(data_)) == 0);
        }
        bool operator!=(const id& other) const
        {
            return !(*this == other);
        }
        const void* data() const { return data_; }

        size_t size() const { return sizeof(data_); }

        bool is_undefined() const
        {
            return (*this == undefined());
        }

        static const wsrep::id& undefined()
        {
            return undefined_;
        }
    private:
        static const wsrep::id undefined_;
        unsigned char data_[16];
    };

    std::ostream& operator<<(std::ostream&, const wsrep::id& id);
    std::istream& operator>>(std::istream&, wsrep::id& id);
}

#endif // WSREP_ID_HPP
