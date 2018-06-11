//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction_context.hpp"
#include "wsrep/provider.hpp"

#include "mock_client_context.hpp"
#include "mock_server_context.hpp"

#include "test_utils.hpp"

#include <boost/test/unit_test.hpp>
#include <boost/mpl/vector.hpp>

namespace
{
    struct replicating_client_fixture_sync_rm
    {
        replicating_client_fixture_sync_rm()
            : sc("s1", "s1", wsrep::server_context::rm_sync)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_context::m_replicating)
            , tc(cc.transaction())
        {
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);
        }
        wsrep::mock_server_context sc;
        wsrep::mock_client_context cc;
        const wsrep::transaction_context& tc;
    };

    struct replicating_client_fixture_async_rm
    {
        replicating_client_fixture_async_rm()
            : sc("s1", "s1", wsrep::server_context::rm_async)
            , cc(sc, wsrep::client_id(1),
                 wsrep::client_context::m_replicating)
            , tc(cc.transaction())
        {
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            // Verify initial state
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);
        }
        wsrep::mock_server_context sc;
        wsrep::mock_client_context cc;
        const wsrep::transaction_context& tc;
    };

    struct applying_client_fixture
    {
        applying_client_fixture()
            : sc("s1", "s1",
                 wsrep::server_context::rm_async)
            , cc(sc,
                 wsrep::client_id(1),
                 wsrep::client_context::m_applier)
            , tc(cc.transaction())
        {
            BOOST_REQUIRE(cc.before_command() == 0);
            BOOST_REQUIRE(cc.before_statement() == 0);
            wsrep::ws_handle ws_handle(1, (void*)1);
            wsrep::ws_meta ws_meta(wsrep::gtid(wsrep::id("1"), 1),
                                   wsrep::stid(sc.id(), 1, cc.id()),
                                   0,
                                   wsrep::provider::flag::start_transaction |
                                   wsrep::provider::flag::commit);
            BOOST_REQUIRE(cc.start_transaction(ws_handle, ws_meta) == 0);
            BOOST_REQUIRE(tc.active() == false);
            BOOST_REQUIRE(cc.start_transaction() == 0);
            BOOST_REQUIRE(tc.active() == true);
            BOOST_REQUIRE(tc.certified() == true);
            BOOST_REQUIRE(tc.ordered() == true);
        }
        wsrep::mock_server_context sc;
        wsrep::mock_client_context cc;
        const wsrep::transaction_context& tc;
    };

    typedef
    boost::mpl::vector<replicating_client_fixture_sync_rm,
                       replicating_client_fixture_async_rm>
    replicating_fixtures;
}


//
// Test a succesful 1PC transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_context_1pc, T,
                                 replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a succesful 2PC transaction lifecycle
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_context_2pc, T,
                                 replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);
    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    // Run before prepare
    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_preparing);

    // Run after prepare
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);

    // Run ordered commit
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_ordered_commit);

    // Run after commit
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a voluntary rollback
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(transaction_context_rollback, T,
                                 replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    // Run before commit
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);

    // Run after commit
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a 1PC transaction which gets BF aborted before before_commit
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_before_before_commit, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    wsrep_test::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

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
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_2pc_bf_before_before_prepare, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    wsrep_test::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.before_prepare());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

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
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_2pc_bf_before_after_prepare, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    // Run before prepare
    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_preparing);

    wsrep_test::bf_abort_unordered(cc);

    // Run before commit
    BOOST_REQUIRE(cc.after_prepare());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

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
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_during_before_commit_uncertified, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    wsrep_test::bf_abort_provider(sc, tc, wsrep::seqno::undefined());

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_cert_failed);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets BF aborted during before_commit
// when waiting for replayers
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_during_commit_wait_for_replayers, T,
    replicating_fixtures, T)
{
    wsrep::mock_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    cc.bf_abort_during_wait_ = true;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction for which prepare data fails
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_error_during_prepare_data, T,
    replicating_fixtures, T)
{
    wsrep::mock_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    cc.error_during_prepare_data_ = true;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

//
// Test a 1PC transaction which gets killed by DBMS before certification
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_killed_before_certify, T,
    replicating_fixtures, T)
{
    wsrep::mock_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    cc.killed_before_certify_ = true;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_interrupted_error);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);

    // Cleanup after statement
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
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_during_before_commit_certified, T,
    replicating_fixtures, T)
{
    wsrep::mock_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    wsrep_test::bf_abort_provider(sc, tc, 1);

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == true);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

//
// Test a transaction which gets BF aborted before before_statement.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_before_before_statement, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);
    cc.after_statement();
    cc.after_command_before_result();
    cc.after_command_after_result();
    cc.before_command();
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(cc.before_statement() == 1);
    BOOST_REQUIRE(tc.active());
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
}

//
// Test a transaction which gets BF aborted before after_statement.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_before_after_statement, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    wsrep_test::bf_abort_unordered(cc);

    cc.after_statement();
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.ordered() == false);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(cc.current_error());
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_abort_after_after_statement, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_exec);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
}

BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_abort_after_after_command_before_result, T,
    replicating_fixtures, T)
{
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_result);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    wsrep_test::bf_abort_unordered(cc);
    // The result is being sent to client. We need to mark transaction
    // as must_abort but not override error yet as this might cause
    // a race condition resulting incorrect result returned to the DBMS client.
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    // After the result has been sent to the DBMS client, the after result
    // processing should roll back the transaction and set the error.
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_idle);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(tc.active() == true);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(
    transaction_context_1pc_bf_abort_after_after_command_after_result_sync_rm,
    replicating_client_fixture_sync_rm)
{
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_result);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_idle);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
}

BOOST_FIXTURE_TEST_CASE(
    transaction_context_1pc_bf_abort_after_after_command_after_result_async_rm,
    replicating_client_fixture_async_rm)
{
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    cc.after_statement();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_exec);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_result);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_idle);
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(cc.before_command() == 1);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_before_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(tc.active() == false);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_1pc_applying,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_ordered_commit);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);
    cc.after_statement();
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_2pc_applying,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.before_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_preparing);
    BOOST_REQUIRE(cc.after_prepare() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_ordered_commit);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);
    cc.after_statement();
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_applying_rollback,
                        applying_client_fixture)
{
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborting);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    cc.after_statement();
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
}
