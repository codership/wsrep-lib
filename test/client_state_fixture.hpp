//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_TEST_CLIENT_CONTEXT_FIXTURE_HPP
#define WSREP_TEST_CLIENT_CONTEXT_FIXTURE_HPP

#include "mock_server_state.hpp"
#include "mock_client_state.hpp"


#include <boost/test/unit_test.hpp>

namespace
{
    struct replicating_client_fixture_sync_rm
    {
        replicating_client_fixture_sync_rm()
            : sc("s1", "s1", wsrep::server_state::rm_sync)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct replicating_client_fixture_async_rm
    {
        replicating_client_fixture_async_rm()
            : sc("s1", "s1", wsrep::server_state::rm_async)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct replicating_client_fixture_2pc
    {
        replicating_client_fixture_2pc()
            : sc("s1", "s1", wsrep::server_state::rm_sync)
            , cc(sc,  wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            cc.do_2pc_ = true;
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct replicating_client_fixture_autocommit
    {
        replicating_client_fixture_autocommit()
            : sc("s1", "s1", wsrep::server_state::rm_sync)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            cc.is_autocommit_ = true;
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct applying_client_fixture
    {
        applying_client_fixture()
            : sc("s1", "s1",
                 wsrep::server_state::rm_async)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_high_priority)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            wsrep::ws_handle ws_handle(1, (void*)1);
            wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                                   wsrep::stid(sc.id(), 1, cc.id()),
                                   wsrep::seqno(0),
                                   wsrep::provider::flag::start_transaction |
                                   wsrep::provider::flag::commit);
            BOOST_REQUIRE(cc.start_transaction(ws_handle, ws_meta) == 0);
            BOOST_REQUIRE(tc.active() == true);
            BOOST_REQUIRE(tc.certified() == true);
            BOOST_REQUIRE(tc.ordered() == true);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct applying_client_fixture_2pc
    {
        applying_client_fixture_2pc()
            : sc("s1", "s1",
                 wsrep::server_state::rm_async)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_high_priority)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            cc.do_2pc_ = true;
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            wsrep::ws_handle ws_handle(1, (void*)1);
            wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                                   wsrep::stid(sc.id(), 1, cc.id()),
                                   wsrep::seqno(0),
                                   wsrep::provider::flag::start_transaction |
                                   wsrep::provider::flag::commit);
            BOOST_REQUIRE(cc.start_transaction(ws_handle, ws_meta) == 0);
            BOOST_REQUIRE(tc.active() == true);
            BOOST_REQUIRE(tc.certified() == true);
            BOOST_REQUIRE(tc.ordered() == true);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct streaming_client_fixture_row
    {
        streaming_client_fixture_row()
            : sc("s1", "s1", wsrep::server_state::rm_sync)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
            cc.enable_streaming(wsrep::streaming_context::row, 1);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct streaming_client_fixture_byte
    {
        streaming_client_fixture_byte()
            : sc("s1", "s1", wsrep::server_state::rm_sync)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
            cc.enable_streaming(wsrep::streaming_context::bytes, 1);
        }
        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };

    struct streaming_client_fixture_statement
    {
        streaming_client_fixture_statement()
            : sc("s1", "s1", wsrep::server_state::rm_sync)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_state::m_replicating)
            , tc(cc.transaction())
        {
            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);
            cc.enable_streaming(wsrep::streaming_context::row, 1);
        }

        wsrep::mock_server_state sc;
        wsrep::mock_client cc;
        const wsrep::transaction& tc;
    };
}
#endif // WSREP_TEST_CLIENT_CONTEXT_FIXTURE_HPP
