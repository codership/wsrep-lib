//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "transaction_context.hpp"
#include "client_context.hpp"
#include "mock_server_context.hpp"
#include "provider.hpp"
#include "mock_provider_impl.hpp"

#include <boost/test/unit_test.hpp>

//
// Utility functions
//
namespace
{
    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_unordered(trrep::client_context& cc,
                            trrep::transaction_context& tc)
    {
        trrep::unique_lock<trrep::mutex> lock(cc.mutex());
        tc.state(lock, trrep::transaction_context::s_must_abort);
    }

    // BF abort method to abort transactions via provider
    void bf_abort_provider(trrep::mock_server_context& sc,
                           const trrep::client_context& cc,
                           const trrep::transaction_context& tc,
                           wsrep_seqno_t seqno)
    {
        sc.mock_provider().bf_abort(cc.id().get(), tc.id().get(), seqno);
    }

}

//
// Test a succesful 1PC transaction lifecycle
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc)
{
    trrep::mock_server_context sc("s1", "s1",
                                  trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

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

}

//
// Test a succesful 2PC transaction lifecycle
//
BOOST_AUTO_TEST_CASE(transaction_context_2pc)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

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
}

//
// Test a 1PC transaction which gets BF aborted before before_commit
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_before_before_commit)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    bf_abort_unordered(cc, tc);

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
}

//
// Test a 2PC transaction which gets BF aborted before before_prepare
//
BOOST_AUTO_TEST_CASE(transaction_context_2pc_bf_before_before_prepare)
{
    trrep::mock_server_context sc("s1", "s1",
                             trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    bf_abort_unordered(cc, tc);

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
}

//
// Test a 2PC transaction which gets BF aborted before before_prepare
//
BOOST_AUTO_TEST_CASE(transaction_context_2pc_bf_before_after_prepare)
{
    trrep::mock_server_context sc("s1", "s1",
                             trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

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

    bf_abort_unordered(cc, tc);

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
}

//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider before the write set was ordered and certified.
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_during_before_commit_uncertified)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    bf_abort_provider(sc, cc, tc, WSREP_SEQNO_UNDEFINED);

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
}


//
// Test a 1PC transaction which gets BF aborted during before_commit via
// provider after the write set was ordered and certified.
//
BOOST_AUTO_TEST_CASE(transaction_context_1pc_bf_during_before_commit_certified)
{
    trrep::mock_server_context sc("s1", "s1",
                                 trrep::server_context::rm_sync);
    trrep::client_context cc(sc, trrep::client_id(1), trrep::client_context::m_replicating);
    trrep::transaction_context tc(cc);

    // Verify initial state
    BOOST_REQUIRE(tc.active() == false);
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    // Start a new transaction with ID 1
    tc.start_transaction(1);
    BOOST_REQUIRE(tc.active());
    BOOST_REQUIRE(tc.id() == trrep::transaction_id(1));
    BOOST_REQUIRE(tc.state() == trrep::transaction_context::s_executing);

    bf_abort_provider(sc, cc, tc, 1);

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
}
