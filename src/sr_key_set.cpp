/*
 * Copyright (C) 2023 Codership Oy <info@codership.com>
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

#include "wsrep/sr_key_set.hpp"

#include "wsrep/key.hpp"

#include <cassert>

void wsrep::sr_key_set::insert(const wsrep::key& key)
{
    assert(key.size() >= 2);
    if (key.size() < 2)
    {
        throw wsrep::runtime_error("Invalid key size");
    }

    root_[std::string(static_cast<const char*>(key.key_parts()[0].data()),
                      key.key_parts()[0].size())]
        .insert(std::string(static_cast<const char*>(key.key_parts()[1].data()),
                            key.key_parts()[1].size()));
}

void wsrep::sr_key_set::clear()
{
    root_.clear();
}
