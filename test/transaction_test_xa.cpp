#include "client_state_fixture.hpp"
#include <iostream>

//
// Test a succesful XA transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE(transaction_xa,
                        replicating_client_fixture_sync_rm)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    cc.assign_xid("test xid");

    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_preparing);
    BOOST_REQUIRE(tc.ordered() == false);
    // certified() only after the last fragment
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    // XA START + PREPARE fragment
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().fragments() == 1);

    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);
    BOOST_REQUIRE(tc.ordered());
    BOOST_REQUIRE(tc.certified());
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    // XA PREPARE and XA COMMIT fragments
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);

    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}


//
// Test a succesful XA transaction lifecycle (applying side)
//
BOOST_FIXTURE_TEST_CASE(transaction_xa_applying,
                        applying_client_fixture)
{
    start_transaction(wsrep::transaction_id(1), wsrep::seqno(1));
    cc.assign_xid("test xid");

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_preparing);
    BOOST_REQUIRE(tc.ordered());
    BOOST_REQUIRE(tc.certified());
    BOOST_REQUIRE(tc.ws_meta().gtid().is_undefined() == false);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);

    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    cc.after_applying();
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

///////////////////////////////////////////////////////////////////////////////
//                       STREAMING REPLICATION                               //
///////////////////////////////////////////////////////////////////////////////

//
// Test a succesful XA transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE(transaction_xa_sr,
                        streaming_client_fixture_byte)
{
    BOOST_REQUIRE(cc.start_transaction(wsrep::transaction_id(1)) == 0);
    cc.assign_xid("test xid");
    cc.bytes_generated_ = 1;
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context().fragments_certified() == 1);
    // XA START fragment with data
    BOOST_REQUIRE(sc.provider().fragments() == 1);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);

    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_executing);

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_preparing);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_prepared);
    // XA PREPARE fragment
    BOOST_REQUIRE(sc.provider().fragments() == 2);

    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_ordered_commit);
    BOOST_REQUIRE(tc.ordered());
    BOOST_REQUIRE(tc.certified());
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction::s_committed);
    BOOST_REQUIRE(cc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    // XA START fragment (with data), XA PREPARE fragment and XA COMMIT fragment
    BOOST_REQUIRE(sc.provider().fragments() == 3);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}
