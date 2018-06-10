//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_utils.hpp"
#include "wsrep/client_context.hpp"
#include "mock_server_context.hpp"


// Simple BF abort method to BF abort unordered transasctions
void wsrep_mock::bf_abort_unordered(wsrep::client_context& cc)
{
    wsrep::unique_lock<wsrep::mutex> lock(cc.mutex());
    assert(cc.transaction().seqno() <= 0);
    cc.bf_abort(lock, 1);
}

    // BF abort method to abort transactions via provider
void wsrep_mock::bf_abort_provider(wsrep::mock_server_context& sc,
                                   const wsrep::transaction_context& tc,
                                   wsrep::seqno bf_seqno)
{
    wsrep::seqno victim_seqno;
    sc.provider().bf_abort(bf_seqno, tc.id(), victim_seqno);
    (void)victim_seqno;
}

void wsrep_mock::start_applying_transaction(
    wsrep::client_context& cc,
    const wsrep::transaction_id& id,
    wsrep::seqno seqno,
    int flags)
{
    wsrep::ws_handle ws_handle(id, 0);
    wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), seqno),
                           wsrep::stid(wsrep::id("1"), id, cc.id()),
                           flags, seqno.get() - 1);
    int ret(cc.start_transaction(ws_handle, ws_meta));
    if (ret != 0)
    {
        throw wsrep::runtime_error("failed to start applying transaction");
    }
}
