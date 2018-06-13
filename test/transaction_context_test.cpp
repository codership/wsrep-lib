//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/transaction_context.hpp"
#include "wsrep/provider.hpp"

#include "test_utils.hpp"
#include "client_context_fixture.hpp"

#include <boost/mpl/vector.hpp>

namespace
{
    typedef
    boost::mpl::vector<replicating_client_fixture_sync_rm,
                       replicating_client_fixture_async_rm>
    replicating_fixtures;
}

BOOST_FIXTURE_TEST_CASE(transaction_context_append_key_data,
                        replicating_client_fixture_sync_rm)
{
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    int vals[3] = {1, 2, 3};
    wsrep::key key(wsrep::key::exclusive);
    for (int i(0); i < 3; ++i)
    {
        key.append_key_part(&vals[i], sizeof(vals[i]));
    }
    BOOST_REQUIRE(cc.append_key(key) == 0);
    wsrep::const_buffer data(&vals[2], sizeof(vals[2]));
    BOOST_REQUIRE(cc.append_data(data) == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    cc.after_statement();
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

    // Verify that the commit can be succesfully executed in separate command
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    cc.after_command_before_result();
    cc.after_command_after_result();
    BOOST_REQUIRE(cc.current_error() == wsrep::e_success);
    BOOST_REQUIRE(cc.before_command() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
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
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider before the write set was ordered and certified.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_during_before_commit_uncertified, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
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
    wsrep::fake_client_context& cc(T::cc);
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
    wsrep::fake_client_context& cc(T::cc);
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
    wsrep::fake_client_context& cc(T::cc);
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
// provider after the write set was ordered and certified. This must
// result replaying of transaction.
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_during_before_commit_certified, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::fake_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    wsrep_test::bf_abort_provider(sc, tc, wsrep::seqno(1));

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
    BOOST_REQUIRE(cc.replays() == 1);
}

//
// Test a 1PC transaction which gets "warning error" from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_warning_error_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_warning;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets transaction missing from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_transaction_missing_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_transaction_missing;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets size exceeded error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_size_exceeded_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_size_exceeded;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets connection failed error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_connection_failed_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_connection_failed;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets not allowed error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_no_allowed_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_not_allowed;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets fatal error from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_fatal_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::fake_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_fatal;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
    BOOST_REQUIRE(cc.aborts() == 1);
}

//
// Test a 1PC transaction which gets unknown from certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_unknown_from_certify, T,
    replicating_fixtures, T)
{
    wsrep::fake_server_context& sc(T::sc);
    wsrep::fake_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    sc.provider().certify_status_ = wsrep::provider::error_unknown;

    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(tc.certified() == false);
    BOOST_REQUIRE(tc.ordered() == false);

    sc.provider().certify_status_ = wsrep::provider::success;

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
    BOOST_REQUIRE(cc.current_error() == wsrep::e_error_during_commit);
}

//
// Test a 1PC transaction which gets BF aborted before grabbing lock
// after certify call
//
BOOST_FIXTURE_TEST_CASE_TEMPLATE(
    transaction_context_1pc_bf_abort_after_certify_regain_lock, T,
    replicating_fixtures, T)
{
    // wsrep::fake_server_context& sc(T::sc);
    wsrep::fake_client_context& cc(T::cc);
    const wsrep::transaction_context& tc(T::tc);

    // Start a new transaction with ID 1
    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == wsrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_executing);

    cc.sync_point_enabled_ = "wsrep_after_certification";
    // Run before commit
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(tc.certified() == true);
    BOOST_REQUIRE(tc.ordered() == true);

    // Rollback sequence
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);

    // Cleanup after statement
    cc.after_statement();
    BOOST_REQUIRE(cc.replays() == 1);
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
    BOOST_REQUIRE(cc.before_command() == 0);
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

BOOST_FIXTURE_TEST_CASE(
    transaction_context_1pc_autocommit_retry_bf_aborted,
    replicating_client_fixture_autocommit)
{

    cc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    wsrep_test::bf_abort_unordered(cc);
    BOOST_REQUIRE(cc.before_commit());
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_may_retry);
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    BOOST_REQUIRE(cc.state() == wsrep::client_context::s_exec);
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

///////////////////////////////////////////////////////////////////////////////
//                       STREAMING REPLICATION                               //
///////////////////////////////////////////////////////////////////////////////

//
// Test 1PC with row streaming with one row
//
BOOST_FIXTURE_TEST_CASE(transaction_context_row_streaming_1pc_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

//
// Test 1PC with row streaming with one row
//
BOOST_FIXTURE_TEST_CASE(transaction_context_row_batch_streaming_1pc_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.enable_streaming(
                      wsrep::streaming_context::row, 2) == 0);
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

//
// Test 1PC row streaming with two separate statements
//
BOOST_FIXTURE_TEST_CASE(
    transaction_context_row_streaming_1pc_commit_two_statements,
    streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 2);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 3);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

//
// Test streaming rollback
//
BOOST_FIXTURE_TEST_CASE(transaction_context_row_streaming_rollback,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);
}

//
// Test streaming certification failure during fragment replication
//
BOOST_FIXTURE_TEST_CASE(transaction_context_row_streaming_cert_fail_non_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    sc.provider().certify_status_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.after_row() == 1);
    sc.provider().certify_status_ = wsrep::provider::success;
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);
}

//
// Test streaming certification failure during commit
//
BOOST_FIXTURE_TEST_CASE(transaction_context_row_streaming_cert_fail_commit,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    sc.provider().certify_status_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.before_commit() == 1);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_cert_failed);
    sc.provider().certify_status_ = wsrep::provider::success;
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_error);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_aborted);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 1);
}

//
// Test streaming BF abort after succesful certification
//
BOOST_FIXTURE_TEST_CASE(transaction_context_row_streaming_bf_abort_committing,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committing);
    wsrep_test::bf_abort_ordered(cc);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_abort);
    BOOST_REQUIRE(cc.before_rollback() == 0);
    BOOST_REQUIRE(cc.after_rollback() == 0);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_must_replay);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(tc.state() == wsrep::transaction_context::s_committed);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}



BOOST_FIXTURE_TEST_CASE(transaction_context_byte_streaming_1pc_commit,
                        streaming_client_fixture_byte)
{
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    cc.bytes_generated_ = 1;
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_byte_batch_streaming_1pc_commit,
                        streaming_client_fixture_byte)
{
    BOOST_REQUIRE(
        cc.enable_streaming(
            wsrep::streaming_context::bytes, 2) == 0);
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    cc.bytes_generated_ = 1;
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    cc.bytes_generated_ = 2;
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}


BOOST_FIXTURE_TEST_CASE(transaction_context_statement_streaming_1pc_commit,
                        streaming_client_fixture_statement)
{
    BOOST_REQUIRE(
        cc.enable_streaming(
            wsrep::streaming_context::statement, 1) == 0);
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_statement_batch_streaming_1pc_commit,
                        streaming_client_fixture_statement)
{
    BOOST_REQUIRE(
        cc.enable_streaming(
            wsrep::streaming_context::statement, 2) == 0);
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 1);
    BOOST_REQUIRE(cc.before_statement() == 0);
    BOOST_REQUIRE(cc.before_commit() == 0);
    BOOST_REQUIRE(cc.ordered_commit() == 0);
    BOOST_REQUIRE(cc.after_commit() == 0);
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_success);
    BOOST_REQUIRE(sc.provider().fragments() == 2);
    BOOST_REQUIRE(sc.provider().start_fragments() == 1);
    BOOST_REQUIRE(sc.provider().commit_fragments() == 1);
}

BOOST_FIXTURE_TEST_CASE(transaction_context_statement_streaming_cert_fail,
                        streaming_client_fixture_row)
{
    BOOST_REQUIRE(
        cc.enable_streaming(
            wsrep::streaming_context::statement, 1) == 0);
    BOOST_REQUIRE(cc.start_transaction(1) == 0);
    BOOST_REQUIRE(cc.after_row() == 0);
    BOOST_REQUIRE(tc.streaming_context_.fragments_certified() == 0);
    sc.provider().certify_status_ = wsrep::provider::error_certification_failed;
    BOOST_REQUIRE(cc.after_statement() == wsrep::client_context::asr_error);
    BOOST_REQUIRE(cc.current_error() == wsrep::e_deadlock_error);
    BOOST_REQUIRE(sc.provider().fragments() == 0);
    BOOST_REQUIRE(sc.provider().start_fragments() == 0);
    BOOST_REQUIRE(sc.provider().rollback_fragments() == 0);
}

///////////////////////////////////////////////////////////////////////////////
//                             misc                                          //
///////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE(transaction_context_state_strings)
{
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::transaction_context::s_executing) == "executing");
    BOOST_REQUIRE(wsrep::to_string(
                      wsrep::transaction_context::s_preparing) == "preparing");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_certifying) == "certifying");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_committing) == "committing");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_ordered_commit) == "ordered_commit");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_committed) == "committed");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_cert_failed) == "cert_failed");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_must_abort) == "must_abort");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_aborting) == "aborting");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_aborted) == "aborted");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_must_replay) == "must_replay");
    BOOST_REQUIRE(
        wsrep::to_string(
            wsrep::transaction_context::s_replaying) == "replaying");
    BOOST_REQUIRE(
        wsrep::to_string(
            static_cast<enum wsrep::transaction_context::state>(0xff)) == "unknown");

}
