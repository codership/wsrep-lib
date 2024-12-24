/*
 * Copyright (C) 2024 Codership Oy <info@codership.com>
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

#include "wsrep/key.hpp"
#include "wsrep/sr_key_set.hpp"
#include <boost/test/unit_test.hpp>

static void append_key_parts(wsrep::key& key,
                             const std::vector<std::string>& parts)
{
    for (const std::string& part : parts)
    {
        key.append_key_part(part.c_str(), part.length());
    }
}

BOOST_AUTO_TEST_CASE(sr_key_set_test_contains)
{
    wsrep::sr_key_set key_set;

    { // contains same key
        wsrep::key key(wsrep::key::exclusive);
        std::vector<std::string> parts = { "1", "2" };
        append_key_parts(key, parts);
        BOOST_REQUIRE(!key_set.contains(key));
        key_set.insert(key);
        BOOST_REQUIRE(key_set.contains(key));
    }

    { // contains same key with different type
        wsrep::key key(wsrep::key::shared);
        std::vector<std::string> parts = { "1", "2" };
        append_key_parts(key, parts);
        BOOST_REQUIRE(key_set.contains(key));
    }

    { // contains same key with one more level
        wsrep::key key(wsrep::key::shared);
        std::vector<std::string> parts = { "1", "2", "3" };
        append_key_parts(key, parts);
        BOOST_REQUIRE(key_set.contains(key));
    }

    { // does not contain different key first at first level
        wsrep::key key(wsrep::key::shared);
        std::vector<std::string> parts = { "different", "2" };
        append_key_parts(key, parts);
        BOOST_REQUIRE(!key_set.contains(key));
    }

    { // does not contain different key part at second level
        wsrep::key key(wsrep::key::shared);
        std::vector<std::string> parts = { "1", "different" };
        append_key_parts(key, parts);
        BOOST_REQUIRE(!key_set.contains(key));
    }
}
