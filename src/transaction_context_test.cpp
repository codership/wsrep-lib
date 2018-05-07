//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "trrep/transaction_context.hpp"
#include "trrep/provider.hpp"

#include "mock_client_context.hpp"
#include "mock_server_context.hpp"

#include "mock_utils.hpp"

#include <boost/test/unit_test.hpp>

//
// Test a succesful 1PC transaction lifecycle
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,trrep::client_id(1),
                             trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before commit
    BOOST_REQUIRE(tc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(tc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(tc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

//
// Test a succesful 2PC transaction lifecycle
//
BOOST_AUTO_TEST_CASE(transaction_context_2pc)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before prepare
    BOOST_REQUIRE(tc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_preparing);

    // Run after prepare
    BOOST_REQUIRE(tc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);

    // Run before commit
    BOOST_REQUIRE(tc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(tc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(tc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

//
// Test a voluntary rollback
//
BOOST_AUTO_TEST_CASE(transaction_context_rollback)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,trrep::client_id(1),
                             trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before commit
    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);

    // Run after commit
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

//
// Test a 1PC transaction which gets BF aborted before before_commit
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_before_before_commit)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_unordered(cc, tc);

    // Run before commit
    BOOST_REQUIRE(tc.before_commit());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 2PC transaction which gets BF aborted before before_prepare
//
BOOST_AUTO_TEST_CASE(transaction_context_2pc_bf_before_before_prepare)
{
    trrep::mock_server_context sc("s1", "s1",
                             trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_unordered(cc, tc);

    // Run before commit
    BOOST_REQUIRE(tc.before_prepare());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 2PC transaction which gets BF aborted before before_prepare
//
BOOST_AUTO_TEST_CASE(transaction_context_2pc_bf_before_after_prepare)
{
    trrep::mock_server_context sc("s1", "s1",
                             trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before prepare
    BOOST_REQUIRE(tc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_preparing);

    trrep_mock::bf_abort_unordered(cc, tc);

    // Run before commit
    BOOST_REQUIRE(tc.after_prepare());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider before the write set was ordered and certified.
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_during_before_commit_uncertified)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_provider(sc, tc, WSREP_SEQNO_UNDEFINED);

    // Run before commit
    BOOST_REQUIRE(tc.before_commit());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_cert_failed);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a transaction which gets BF aborted before after_statement.
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_during_before_after_statement)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_unordered(cc, tc);

    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider after the write set was ordered and certified.
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_during_before_commit_certified)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context& tc(cc.transaction());

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_provider(sc, tc, 1);

    // Run before commit
    BOOST_REQUIRE(tc.before_commit());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == true);

    // Rollback sequence
    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_replay);

    // Cleanup after statement
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

BOOST_AUTO_TEST_CASE(transaction_context_1pc_applying)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                             trrep::client_id(1),
                             trrep::client_context::m_applier);
    trrep::transaction_context& tc(trrep_mock::start_applying_transaction(
                                       cc, 1, 1,
                                       WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END));

    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.start_transaction() == 0);
    BOOST_REQUIRE(tc.active() == true);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    BOOST_REQUIRE(tc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);
    BOOST_REQUIRE(tc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);
    BOOST_REQUIRE(tc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

BOOST_AUTO_TEST_CASE(transaction_context_2pc_applying)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                             trrep::client_id(1),
                             trrep::client_context::m_applier);
    trrep::transaction_context& tc(trrep_mock::start_applying_transaction(
                                       cc, 1, 1,
                                       WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END));

    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.start_transaction() == 0);
    BOOST_REQUIRE(tc.active() == true);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    BOOST_REQUIRE(tc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_preparing);
    BOOST_REQUIRE(tc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);
    BOOST_REQUIRE(tc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);
    BOOST_REQUIRE(tc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);
    BOOST_REQUIRE(tc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

BOOST_AUTO_TEST_CASE(transaction_context_applying_rollback)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::mock_client_context cc(sc,
                             trrep::client_id(1),
                             trrep::client_context::m_applier);
    trrep::transaction_context& tc(trrep_mock::start_applying_transaction(
                                       cc, 1, 1,
                                       WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END));

    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.start_transaction() == 0);
    BOOST_REQUIRE(tc.active() == true);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    BOOST_REQUIRE(tc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(tc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);
    BOOST_REQUIRE(tc.after_statement() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}
