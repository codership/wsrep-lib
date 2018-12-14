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

#include "mock_server_state.hpp"

#include <boost/test/unit_test.hpp>

namespace
{
    struct server_fixture_base
    {
        server_fixture_base()
            : server_service(ss)
            , ss("s1",
                 wsrep::server_state::rm_sync, server_service)
            , cc(ss,
                 wsrep::client_id(1),
                 wsrep::client_state::m_high_priority)
            , hps(ss, &cc, false)
            , ws_handle(wsrep::transaction_id(1), (void*)1)
            , ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                      wsrep::stid(wsrep::id("1"), wsrep::transaction_id(1),
                                  wsrep::client_id(1)),
                      wsrep::seqno(0),
                      wsrep::provider::flag::start_transaction |
                      wsrep::provider::flag::commit)
            , cluster_id("1")
            , bootstrap_view()
        {
            wsrep::gtid state_id(cluster_id, wsrep::seqno(0));
            std::vector<wsrep::view::member> members;
            members.push_back(wsrep::view::member(wsrep::id("s1"), "name", ""));
            bootstrap_view = wsrep::view(state_id,
                                         wsrep::seqno(1),
                                         wsrep::view::primary,
                                         0, // capabilities
                                         0, // own index
                                         1, // protocol version
                                         members);

            cc.open(cc.id());
            BOOST_REQUIRE(cc.before_command() == 0);
        }
        wsrep::mock_server_service server_service;
        wsrep::mock_server_state ss;
        wsrep::mock_client cc;
        wsrep::mock_high_priority_service hps;
        wsrep::ws_handle ws_handle;
        wsrep::ws_meta ws_meta;
        wsrep::id cluster_id;
        wsrep::view bootstrap_view;
    };

    struct applying_server_fixture : server_fixture_base
    {
        applying_server_fixture()
            : server_fixture_base()
        {
            ss.mock_connect();
        }
    };

    struct sst_first_server_fixture : server_fixture_base
    {
        sst_first_server_fixture()
            : server_fixture_base()
        {
            server_service.sst_before_init_ = true;
        }
    };

    struct init_first_server_fixture : server_fixture_base
    {
        init_first_server_fixture()
            : server_fixture_base()
        {
            server_service.sst_before_init_ = false;
        }
    };
}

// Test on_apply() method for 1pc
BOOST_FIXTURE_TEST_CASE(server_state_applying_1pc,
                        applying_server_fixture)
{
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 0);
    const wsrep::transaction& txc(cc.transaction());
    // ::abort();
    BOOST_REQUIRE_MESSAGE(
        txc.state() == wsrep::transaction::s_committed,
        "Transaction state " << txc.state() << " not committed");
}

// Test on_apply() method for 2pc
BOOST_FIXTURE_TEST_CASE(server_state_applying_2pc,
                        applying_server_fixture)
{
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 0);
    const wsrep::transaction& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction::s_committed);
}

// Test on_apply() method for 1pc transaction which
// fails applying and rolls back
BOOST_FIXTURE_TEST_CASE(server_state_applying_1pc_rollback,
                        applying_server_fixture)
{
    hps.fail_next_applying_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 1);
    const wsrep::transaction& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction::s_aborted);
}

// Test on_apply() method for 2pc transaction which
// fails applying and rolls back
BOOST_FIXTURE_TEST_CASE(server_state_applying_2pc_rollback,
                        applying_server_fixture)
{
    hps.fail_next_applying_ = true;
    char buf[1] = { 1 };
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer(buf, 1)) == 1);
    const wsrep::transaction& txc(cc.transaction());
    BOOST_REQUIRE(txc.state() == wsrep::transaction::s_aborted);
}

BOOST_FIXTURE_TEST_CASE(server_state_streaming, applying_server_fixture)
{
    ws_meta = wsrep::ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(1)),
                             wsrep::stid(wsrep::id("1"),
                                         wsrep::transaction_id(1),
                                         wsrep::client_id(1)),
                             wsrep::seqno(0),
                             wsrep::provider::flag::start_transaction);
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer("1", 1)) == 0);
    BOOST_REQUIRE(ss.find_streaming_applier(
                      ws_meta.server_id(), ws_meta.transaction_id()));
    ws_meta = wsrep::ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
                             wsrep::stid(wsrep::id("1"),
                                         wsrep::transaction_id(1),
                                         wsrep::client_id(1)),
                             wsrep::seqno(1),
                             0);
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer("1", 1)) == 0);
    ws_meta = wsrep::ws_meta(wsrep::gtid(wsrep::id("1"), wsrep::seqno(2)),
                             wsrep::stid(wsrep::id("1"),
                                         wsrep::transaction_id(1),
                                         wsrep::client_id(1)),
                             wsrep::seqno(1),
                             wsrep::provider::flag::commit);
    BOOST_REQUIRE(ss.on_apply(hps, ws_handle, ws_meta,
                              wsrep::const_buffer("1", 1)) == 0);
    BOOST_REQUIRE(ss.find_streaming_applier(
                      ws_meta.server_id(), ws_meta.transaction_id()) == 0);
}


BOOST_AUTO_TEST_CASE(server_state_state_strings)
{
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_disconnected) == "disconnected");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_initializing) == "initializing");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_initialized) == "initialized");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_connected) == "connected");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_joiner) == "joiner");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_joined) == "joined");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_donor) == "donor");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_synced) == "synced");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::server_state::s_disconnecting) == "disconnecting");
    BOOST_REQUIRE(wsrep::to_string(
                      static_cast<enum wsrep::server_state::state>(0xff)) == "unknown");
}

///////////////////////////////////////////////////////////////////////////////
//                     Test cases for SST first                              //
///////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(server_state_sst_first_boostrap,
                        sst_first_server_fixture)
{
    BOOST_REQUIRE(ss.connect("cluster", "local", "0", false) == 0);
    ss.on_connect(bootstrap_view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    server_service.sync_point_enabled_ = "on_view_wait_initialized";
    server_service.sync_point_action_  = server_service.spa_initialize;
    ss.on_view(bootstrap_view, &hps);
    server_service.sync_point_enabled_ = "";
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}

///////////////////////////////////////////////////////////////////////////////
//                     Test cases for init first                             //
///////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_CASE(server_state_init_first_boostrap,
                        init_first_server_fixture)
{
    ss.initialized();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_initialized);
    BOOST_REQUIRE(ss.connect("cluster", "local", "0", false) == 0);
    ss.on_connect(bootstrap_view);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_connected);
    ss.on_view(bootstrap_view, &hps);
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_joined);
    ss.on_sync();
    BOOST_REQUIRE(ss.state() == wsrep::server_state::s_synced);
}
