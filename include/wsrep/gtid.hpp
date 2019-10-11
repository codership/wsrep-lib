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
#include "compiler.hpp"

#include <iosfwd>

/**
 * Minimum number of bytes guaratneed to store GTID string representation,
 * terminating '\0' not included (36 + 1 + 20).
 */
#define WSREP_LIB_GTID_C_STR_LEN 57

namespace wsrep
{
    class gtid
    {
    public:
        gtid()
            : id_()
            , seqno_()
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
        static const wsrep::gtid& undefined()
        {
            return undefined_;
        }
        bool operator==(const gtid& other) const
        {
            return (
                seqno_ == other.seqno_ &&
                id_ == other.id_
            );
        }
    private:
        static const wsrep::gtid undefined_;
        wsrep::id id_;
        wsrep::seqno seqno_;
    };

    /**
     * Scan a GTID from C string.
     *
     * @param buf Buffer containing the string
     * @param len Length of buffer
     * @param[out] gtid Gtid to be printed to
     *
     * @return Number of bytes scanned, negative value on error.
     */
    ssize_t scan_from_c_str(const char* buf, size_t buf_len,
                            wsrep::gtid& gtid);

    /**
     * Print a GTID into character buffer.
     * @param buf Pointer to the beginning of the buffer
     * @param buf_len Buffer length
     *
     * @return Number of characters printed or negative value for error
     */
    ssize_t print_to_c_str(const wsrep::gtid&, char* buf, size_t buf_len);

    /**
     * Overload for ostream operator<<.
     */
    std::ostream& operator<<(std::ostream&, const wsrep::gtid&);

    /**
     * Overload for istream operator>>.
     */
    std::istream& operator>>(std::istream&, wsrep::gtid&);
}

#endif // WSREP_GTID_HPP
