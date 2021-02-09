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

#include "wsrep/provider_options.hpp"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(provider_options_parse_empty)
{
    bool called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "",
            [&called](const std::string&, const std::string&)
            {
                called = true;
            }) == 0);
    BOOST_REQUIRE(not called);
}

BOOST_AUTO_TEST_CASE(provider_options_parse_one_value)
{
    size_t called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "key = value",
            [&called](const std::string& key, const std::string& value)
            {
                BOOST_REQUIRE(key == "key");
                BOOST_REQUIRE(value == "value");
                ++called;
            }) == 0);
    BOOST_REQUIRE(called == 1);
}

BOOST_AUTO_TEST_CASE(provider_options_parse_two_values)
{
    size_t called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "key1 = value1; key2 = value2;",
            [&called](const std::string&, const std::string&)
            {
                ++called;
            }) == 0);
    BOOST_REQUIRE(called == 2);
}

BOOST_AUTO_TEST_CASE(provider_options_parse_empty_values)
{
    size_t called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "key1 = ; key2 = ;",
            [&called](const std::string&, const std::string&)
            {
                ++called;
            }) == 0);
    BOOST_REQUIRE(called == 2);
}

BOOST_AUTO_TEST_CASE(provider_options_parse_empty_values_without_space)
{
    size_t called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "key1=; key2=;",
            [&called](const std::string&, const std::string&)
            {
                ++called;
            }) == 0);
    BOOST_REQUIRE(called == 2);
}

// TODO: Currently parameter without = separator is parsed as
// parameter with empty value, fix this.
BOOST_AUTO_TEST_CASE(provider_options_parse_no_value)
{
    bool called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "no_value",
            [&called](const std::string&, const std::string&)
            {
                called = true;
            }) == 0);
    BOOST_REQUIRE(called);
}

BOOST_AUTO_TEST_CASE(provider_options_parse_escaped)
{
    bool called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "key\\; = \\;value",
            [&called](const std::string& key, const std::string& value)
            {
                BOOST_REQUIRE_MESSAGE(key == "key;",
                                      key << " != key;");
                BOOST_REQUIRE(value == ";value");
                called = true;
            }) == 0);
    BOOST_REQUIRE(called);
}

BOOST_AUTO_TEST_CASE(provider_options_parse_escape_last)
{
    bool called{};
    BOOST_REQUIRE(
        wsrep::parse_provider_options(
            "key\\",
            [&called](const std::string&, const std::string&)
            {
                called = false;
            }) == 0);
    BOOST_REQUIRE(not called);
}
