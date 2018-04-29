//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_utils.hpp"
#include "client_context.hpp"
#include "mock_server_context.hpp"


// Simple BF abort method to BF abort unordered transasctions
void trrep_mock::bf_abort_unordered(trrep::client_context& cc,
                                    trrep::transaction_context& tc)
{
    trrep::unique_lock<trrep::mutex> lock(cc.mutex());
    tc.state(lock, trrep::transaction_context::s_must_abort);
}

    // BF abort method to abort transactions via provider
void trrep_mock::bf_abort_provider(trrep::mock_server_context& sc,
                                   const trrep::transaction_context& tc,
                                   wsrep_seqno_t bf_seqno)
{
    wsrep_seqno_t victim_seqno;
    sc.provider().bf_abort(bf_seqno, tc.id().get(), &victim_seqno);
    (void)victim_seqno;
}

trrep::transaction_context& trrep_mock::start_applying_transaction(
    trrep::client_context& cc,
    const trrep::transaction_id& id,
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
        throw trrep::runtime_error("failed to start applying transaction");
    }
    return cc.transaction();
}
