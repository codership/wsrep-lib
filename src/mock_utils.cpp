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
                                   wsrep_seqno_t bf_seqno)
{
    wsrep_seqno_t victim_seqno;
    sc.provider().bf_abort(bf_seqno, tc.id().get(), &victim_seqno);
    (void)victim_seqno;
}

void wsrep_mock::start_applying_transaction(
    wsrep::client_context& cc,
    const wsrep::transaction_id& id,
    wsrep_seqno_t seqno,
    uint32_t flags)
{
    wsrep_ws_handle_t ws_handle = { id.get(), 0 };
    wsrep_trx_meta_t meta = {
        { {1 }, seqno }, /* gtid */
        { { static_cast<uint8_t>(cc.id().get()) }, id.get(), cc.id().get() }, /* stid */
        seqno - 1
    };
    int ret(cc.start_transaction(ws_handle, meta, flags));
    if (ret != 0)
    {
        throw wsrep::runtime_error("failed to start applying transaction");
    }
}
