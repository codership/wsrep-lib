//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "trrep/transaction_context.hpp"
#include "trrep/provider.hpp"

#include "mock_client_context.hpp"
#include "mock_server_context.hpp"

#include "mock_utils.hpp"

#include <boost/test/unit_test.hpp>

namespace
{
    struct replicating_client_fixture
    {
        replicating_client_fixture()
            : sc("s1", "s1", trrep::server_context::rm_async)
            , cc(sc, trrep::client_id(1),
                 trrep::client_context::m_replicating)
            , tc(cc.transaction())
        {
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);
        }
        trrep::mock_server_context sc;
        trrep::mock_client_context cc;
        const trrep::transaction_context& tc;
    };

    struct applying_client_fixture
    {
        applying_client_fixture()
            : sc("s1", "s1",
                 trrep::server_context::rm_async)
            , cc(sc,
                 trrep::client_id(1),
                 trrep::client_context::m_applier)
            , tc(cc.transaction())
        {
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            trrep_mock::start_applying_transaction(
                cc, 1, 1,
                WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(cc.start_transaction() == 0);
            BOOST_REQUIRE(tc.active() == true);
            BOOST_REQUIRE(tc.certified() == true);
            BOOST_REQUIRE(tc.ordered() == true);
        }
        trrep::mock_server_context sc;
        trrep::mock_client_context cc;
        const trrep::transaction_context& tc;
    };

}

//
// Test a succesful 1PC transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE(transaction_context_1pc, replicating_client_fixture)
{


    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

//
// Test a succesful 2PC transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE(transaction_context_2pc, replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before prepare
    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_preparing);

    // Run after prepare
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

//
// Test a voluntary rollback
//
BOOST_FIXTURE_TEST_CASE(transaction_context_rollback, replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before commit
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);

    // Run after commit
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

//
// Test a 1PC transaction which gets BF aborted before before_commit
//
BOOST_FIXTURE_TEST_CASE(transaction_context_1pc_bf_before_before_commit,
                        replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 2PC transaction which gets BF aborted before before_prepare
//
BOOST_FIXTURE_TEST_CASE(transaction_context_2pc_bf_before_before_prepare,
                        replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.before_prepare());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 2PC transaction which gets BF aborted before before_prepare
//
BOOST_FIXTURE_TEST_CASE(transaction_context_2pc_bf_before_after_prepare,
                        replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Run before prepare
    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_preparing);

    trrep_mock::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.after_prepare());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider before the write set was ordered and certified.
//
BOOST_FIXTURE_TEST_CASE(
    transaction_context_1pc_bf_during_before_commit_uncertified,
    replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_provider(sc, tc, WSREP_SEQNO_UNDEFINED);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_cert_failed);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a transaction which gets BF aborted before after_statement.
//
BOOST_FIXTURE_TEST_CASE(
    transaction_context_1pc_bf_during_before_after_statement,
    replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_unordered(cc);

    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider after the write set was ordered and certified.
//
BOOST_FIXTURE_TEST_CASE(
    transaction_context_1pc_bf_during_before_commit_certified,
    replicating_client_fixture)
{
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    trrep_mock::bf_abort_provider(sc, tc, 1);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == true);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_must_replay);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_1pc_applying,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    cc.after_statement();
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

    BOOST_REQUIRE(cc.before_command() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);

    trrep_mock::start_applying_transaction(
        cc, 1, 1,
        WSREP_FLAG_TRX_START | WSREP_FLAG_TRX_END);
    const trrep::transaction_context& tc(cc.transaction());

    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.start_transaction() == 0);
    BOOST_REQUIRE(tc.active() == true);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_preparing);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_ordered_commit);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    cc.after_statement();
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_applying_rollback,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);
    cc.after_statement();
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_aborted);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == trrep::e_success);
}
