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

void wsrep::view::print(std::ostream& os) const
{
    os << "  id: " << state_id() << "\n"
       << "  status: " << status() << "\n"
       << "  prococol_version: " << protocol_version() << "\n"
       << "  final: " << final() << "\n"
       << "  own_index: " << own_index() << "\n"
       << "  members(" << members().size() << "):\n";

    for (std::vector<wsrep::view::member>::const_iterator i(members().begin());
         i != members().end(); ++i)
    {
        os << "\t" << (i - members().begin()) /* ordinal index */
           << ") id: " << i->id()
           << ", name: " << i->name() << "\n";
    }
}
