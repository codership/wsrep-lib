//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_server_context.hpp"
#include "mock_utils.hpp"

#include <boost/test/unit_test.hpp>

// Test on_apply() method for 1pc
BOOST_AUTO_TEST_CASE(server_context_applying_1pc)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                                  trrep::client_id(1),
                                  trrep::client_context::m_applier,
                                  false);
    trrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, trrep::data(buf, 1)) == 0);
    const trrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == trrep::transaction_context::s_committed);
}

// Test on_apply() method for 2pc
BOOST_AUTO_TEST_CASE(server_context_applying_2pc)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                                  trrep::client_id(1),
                                  trrep::client_context::m_applier,
                                  true);
    trrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, trrep::data(buf, 1)) == 0);
    const trrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == trrep::transaction_context::s_committed);
}

// Test on_apply() method for 1pc transaction which
// fails applying and rolls back
BOOST_AUTO_TEST_CASE(server_context_applying_1pc_rollback)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                                  trrep::client_id(1),
                                  trrep::client_context::m_applier,
                                  false);
    cc.fail_next_applying(true);
    trrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };

    BOOST_REQUIRE(sc.on_apply(cc, trrep::data(buf, 1)) == 1);
    const trrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == trrep::transaction_context::s_aborted);
}

// Test on_apply() method for 2pc transaction which
// fails applying and rolls back
BOOST_AUTO_TEST_CASE(server_context_applying_2pc_rollback)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                                  trrep::client_id(1),
                                  trrep::client_context::m_applier,
                                  true);
    cc.fail_next_applying(true);
    trrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, trrep::data(buf, 1)) == 1);
    const trrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == trrep::transaction_context::s_aborted);
}
