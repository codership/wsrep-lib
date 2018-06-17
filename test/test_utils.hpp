//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

// Forward declarations
namespace wsrep
{
    class client_state;
    class mock_server_state;
}

#include "wsrep/transaction.hpp"
#include "wsrep/provider.hpp"

//
// Utility functions
//
namespace wsrep_test
{

    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_unordered(wsrep::client_state& cc);

    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_ordered(wsrep::client_state& cc);

    // BF abort method to abort transactions via provider
    void bf_abort_provider(wsrep::mock_server_state& sc,
                           const wsrep::transaction& tc,
                           wsrep::seqno bf_seqno);

}
