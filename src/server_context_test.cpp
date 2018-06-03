//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_server_context.hpp"
#include "mock_utils.hpp"

#include <boost/test/unit_test.hpp>

// Test on_apply() method for 1pc
BOOST_AUTO_TEST_CASE(server_context_applying_1pc)
{
    wsrep::mock_server_context sc("s1", "s1",
                                  wsrep::server_context::rm_sync);
    wsrep::mock_client_context cc(sc,
                                  wsrep::client_id(1),
                                  wsrep::client_context::m_applier,
                                  false);
    wsrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, wsrep::data(buf, 1)) == 0);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_committed);
}

// Test on_apply() method for 2pc
BOOST_AUTO_TEST_CASE(server_context_applying_2pc)
{
    wsrep::mock_server_context sc("s1", "s1",
                                  wsrep::server_context::rm_sync);
    wsrep::mock_client_context cc(sc,
                                  wsrep::client_id(1),
                                  wsrep::client_context::m_applier,
                                  true);
    wsrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, wsrep::data(buf, 1)) == 0);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_committed);
}

// Test on_apply() method for 1pc transaction which
// fails applying and rolls back
BOOST_AUTO_TEST_CASE(server_context_applying_1pc_rollback)
{
    wsrep::mock_server_context sc("s1", "s1",
                                  wsrep::server_context::rm_sync);
    wsrep::mock_client_context cc(sc,
                                  wsrep::client_id(1),
                                  wsrep::client_context::m_applier,
                                  false);
    cc.fail_next_applying(true);
    wsrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };

    BOOST_REQUIRE(sc.on_apply(cc, wsrep::data(buf, 1)) == 1);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_aborted);
}

// Test on_apply() method for 2pc transaction which
// fails applying and rolls back
BOOST_AUTO_TEST_CASE(server_context_applying_2pc_rollback)
{
    wsrep::mock_server_context sc("s1", "s1",
                                  wsrep::server_context::rm_sync);
    wsrep::mock_client_context cc(sc,
                                  wsrep::client_id(1),
                                  wsrep::client_context::m_applier,
                                  true);
    cc.fail_next_applying(true);
    wsrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    char buf[1] = { 1 };
    BOOST_REQUIRE(sc.on_apply(cc, wsrep::data(buf, 1)) == 1);
    const wsrep::transaction_context& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction_context::s_aborted);
}
