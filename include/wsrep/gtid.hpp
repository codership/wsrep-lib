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

#ifndef WSREP_GTID_HPP
#define WSREP_GTID_HPP

#include "id.hpp"
#include "seqno.hpp"

#include <iosfwd>

namespace wsrep
{
    class gtid
    {
    public:
        gtid()
            : id_(wsrep::id::undefined())
            , seqno_(wsrep::seqno::undefined())
        { }
        gtid(const wsrep::id& id, wsrep::seqno seqno)
            : id_(id)
            , seqno_(seqno)
        { }
        const wsrep::id& id() const { return id_; }
        wsrep::seqno seqno() const { return seqno_ ; }
        bool is_undefined() const
        {
            return (seqno_.is_undefined() && id_.is_undefined());
        }
        static wsrep::gtid undefined()
        {
            static const wsrep::gtid ret(wsrep::id::undefined(),
                                         wsrep::seqno::undefined());
            return ret;
        }
    private:
        wsrep::id id_;
        wsrep::seqno seqno_;
    };

    /**
     * Print a GTID into character buffer.
     * @param buf Pointer to the beginning of the buffer
     * @param buf_len Buffer length
     *
     * @return Number of characters printed or negative value for error
     */
    ssize_t gtid_print_to_c_str(const wsrep::gtid&, char* buf, size_t buf_len);
    /**
     * Return minimum number of bytes guaranteed to store GTID string
     * representation, terminating '\0' not included (36 + 1 + 20)
     */
    static inline size_t gtid_c_str_len()
    {
        return 57;
    }
    std::ostream& operator<<(std::ostream&, const wsrep::gtid&);
    std::istream& operator>>(std::istream&, wsrep::gtid&);
}

#endif // WSREP_GTID_HPP
