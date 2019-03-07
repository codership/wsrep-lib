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

#include "wsrep/client_state.hpp"

#include "client_state_fixture.hpp"

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_CASE(test_local_nbo,
                        replicating_client_fixture_sync_rm)
{
    // NBO is executed in two consecutive TOI operations
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    // First phase certifies the write set and enters TOI
    wsrep::key key(wsrep::key::exclusive);
    key.append_key_part("k1", 2);
    key.append_key_part("k2", 2);
    wsrep::key_array keys{key};
    std::string data("data");
    BOOST_REQUIRE(cc.begin_nbo_phase_one(
                      keys,
                      wsrep::const_buffer(data.data(),
                                          data.size())) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(cc.in_toi());
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);
    // After required resoureces have been grabbed, NBO leave should
    // end TOI but leave the client state in m_nbo.
    BOOST_REQUIRE(cc.end_nbo_phase_one() == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(cc.in_toi() == false);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);
    // Second phase replicates the NBO end event and grabs TOI
    // again for finalizing the NBO.
    BOOST_REQUIRE(cc.begin_nbo_phase_two(keys) == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_nbo);
    BOOST_REQUIRE(cc.in_toi());
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);
    // End of NBO phase will leave TOI and put the client state back to
    // m_local
    BOOST_REQUIRE(cc.end_nbo_phase_two() == 0);
    BOOST_REQUIRE(cc.mode() == wsrep::client_state::m_local);
    BOOST_REQUIRE(cc.in_toi() == false);
    BOOST_REQUIRE(cc.toi_mode() == wsrep::client_state::m_local);

    // There must have been two toi write sets, one with
    // start transaction flag, another with commit flag.
    BOOST_REQUIRE(sc.provider().toi_write_sets() == 2);
    BOOST_REQUIRE(sc.provider().toi_start_transaction() == 1);
    BOOST_REQUIRE(sc.provider().toi_commit() == 1);
}
