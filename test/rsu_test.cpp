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

#include "wsrep/client_state.hpp"

#include "client_state_fixture.hpp"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_CASE(test_rsu,
                        replicating_client_fixture_sync_rm)
{
    BOOST_REQUIRE(cc.begin_rsu(1) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_rsu);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);

    BOOST_REQUIRE(cc.end_rsu() == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_undefined);
}
