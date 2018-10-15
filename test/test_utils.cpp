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

#include "test_utils.hpp"
#include "wsrep/client_state.hpp"
#include "mock_server_state.hpp"


// Simple BF abort method to BF abort unordered transasctions
void wsrep_test::bf_abort_unordered(wsrep::client_state& cc)
{
    assert(cc.transaction().ordered() == false);
    cc.bf_abort(wsrep::seqno(1));
}

void wsrep_test::bf_abort_ordered(wsrep::client_state& cc)
{
    assert(cc.transaction().ordered());
    cc.bf_abort(wsrep::seqno(0));
}
// BF abort method to abort transactions via provider
void wsrep_test::bf_abort_provider(wsrep::mock_server_state& sc,
                                   const wsrep::transaction& tc,
                                   wsrep::seqno bf_seqno)
{
    wsrep::seqno victim_seqno;
    sc.provider().bf_abort(bf_seqno, tc.id(), victim_seqno);
    (void)victim_seqno;
}
