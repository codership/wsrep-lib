//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

// Forward declarations
namespace wsrep
{
    class client_context;
    class mock_server_context;
}

#include "wsrep/transaction_context.hpp"
#include "wsrep/provider.hpp"

//
// Utility functions
//
namespace wsrep_test
{

    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_unordered(wsrep::client_context& cc);

    // BF abort method to abort transactions via provider
    void bf_abort_provider(wsrep::mock_server_context& sc,
                           const wsrep::transaction_context& tc,
                           wsrep::seqno bf_seqno);

}
