//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "fake_server_context.hpp"

#include <boost/test/unit_test.hpp>

namespace
{
    struct applying_server_fixture
    {
        applying_server_fixture()
            : sc("s1", "s1",
                 wsrep::server_context::rm_sync)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_context::m_applier,
                 false)
            , ws_handle(1, (void*)1)
            , ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                      wsrep::stid(wsrep::id("1"), 1, 1),
                      wsrep::seqno(0),
                      wsrep::provider::flag::start_transaction |
                      wsrep::provider::flag::commit)
        {
            cc.start_transaction(ws_handle, ws_meta);
        }
        wsrep::fake_server_context sc;
        wsrep::fake_client_context cc;
        wsrep::ws_handle ws_handle;
        wsrep::ws_meta ws_meta;
    };
}

// Test on_apply() method for 1pc
BOOST_FIXTURE_TEST_CASE(server_context_applying_1pc,
                        applying_server_fixture)
{
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 0);
    const wsrep::transaction_context& txc(cc.transaction());
    // ::abort();
    BOOST_REQUIRE_MESSAGE(
        txc.state() == wsrep::transaction_context::s_committed,
        "Transaction state " << txc.state() << " not committed");
}

// Test on_apply() method for 2pc
BOOST_FIXTURE_TEST_CASE(server_context_applying_2pc,
                        applying_server_fixture)
{
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 0);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_committed);
}

// Test on_apply() method for 1pc transaction which
// fails applying and rolls back
BOOST_FIXTURE_TEST_CASE(server_context_applying_1pc_rollback,
                        applying_server_fixture)
{
    cc.fail_next_applying_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 1);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_aborted);
}

// Test on_apply() method for 2pc transaction which
// fails applying and rolls back
BOOST_FIXTURE_TEST_CASE(server_context_applying_2pc_rollback,
                        applying_server_fixture)
{
    cc.fail_next_applying_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 1);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_aborted);
}
