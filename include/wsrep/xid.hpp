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

#ifndef WSREP_XID_HPP
#define WSREP_XID_HPP

#include <iosfwd>
#include <string>

namespace wsrep
{
    class xid
    {
    public:
        xid() : xid_() { }
        xid(const std::string& str) : xid_(str) { }
        xid(const char* s) : xid_(s) { }
        bool empty() const { return xid_.empty(); }
        void clear() { xid_.clear(); }
        bool operator==(const xid& other) const { return xid_ == other.xid_; }
        friend std::string to_string(const wsrep::xid& xid);
        friend std::ostream& operator<<(std::ostream& os, const wsrep::xid& xid);
    private:
        std::string xid_;
    };

    std::string to_string(const wsrep::xid& xid);
    std::ostream& operator<<(std::ostream& os, const wsrep::xid& xid);
}

#endif // WSREP_XID_HPP
