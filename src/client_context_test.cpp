
//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_client_context.hpp"
#include "mock_server_context.hpp"
#include "mock_utils.hpp"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_CASE(client_context_test_error_codes)
{
    trrep::mock_server_context sc("s1", "s1", trrep::server_context::rm_async);
    trrep::mock_client_context cc(sc,
                                  trrep::client_id(1),
                                  trrep::client_context::m_applier,
                                  false);
    const trrep::transaction_context& txc(cc.transaction());
    cc.before_command();
    cc.before_statement();

    BOOST_REQUIRE(txc.active() == false);
    cc.start_transaction(1);
    trrep_mock::bf_abort_unordered(cc);

    cc.after_statement();
    cc.after_command();
}
