//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "test_utils.hpp"
#include "wsrep/client_context.hpp"
#include "mock_server_context.hpp"


// Simple BF abort method to BF abort unordered transasctions
void wsrep_test::bf_abort_unordered(wsrep::client_context& cc)
{
    assert(cc.transaction().ordered() == false);
    cc.bf_abort(wsrep::seqno(1));
}

void wsrep_test::bf_abort_ordered(wsrep::client_context& cc)
{
    assert(cc.transaction().ordered());
    cc.bf_abort(wsrep::seqno(0));
}
// BF abort method to abort transactions via provider
void wsrep_test::bf_abort_provider(wsrep::mock_server_context& sc,
                                   const wsrep::transaction_context& tc,
                                   wsrep::seqno bf_seqno)
{
    wsrep::seqno victim_seqno;
    sc.provider().bf_abort(bf_seqno, tc.id(), victim_seqno);
    (void)victim_seqno;
}
