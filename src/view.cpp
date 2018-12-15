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

#include "wsrep/view.hpp"
#include "wsrep/provider.hpp"

int wsrep::view::member_index(const wsrep::id& member_id) const
{
    // first, quick guess
    if (own_index_ >= 0 && members_[own_index_].id() == member_id)
    {
        return own_index_;
    }

    // guesing didn't work, scan the list
    for (size_t i(0); i < members_.size(); ++i)
    {
        if (static_cast<int>(i) != own_index_ && members_[i].id() == member_id)
        {
            return i;
        }
    }

    return -1;
}

static const char* view_status_str(enum wsrep::view::status s)
{
    switch(s)
    {
        case wsrep::view::primary:      return "PRIMARY";
        case wsrep::view::non_primary:  return "NON-PRIMARY";
        case wsrep::view::disconnected: return "DISCONNECTED";
    }

    assert(0);
    return "invalid status";
}

void wsrep::view::print(std::ostream& os) const
{
    os << "  id: " << state_id() << "\n"
       << "  status: " << view_status_str(status()) << "\n"
       << "  prococol_version: " << protocol_version() << "\n"
       << "  capabilities: " << provider::capability::str(capabilities())<<"\n"
       << "  final: " << (final() ? "yes" : "no") << "\n"
       << "  own_index: " << own_index() << "\n"
       << "  members(" << members().size() << "):\n";

    for (std::vector<wsrep::view::member>::const_iterator i(members().begin());
         i != members().end(); ++i)
    {
        os << "\t" << (i - members().begin()) /* ordinal index */
           << ": " << i->id()
           << ", " << i->name() << "\n";
    }
}
