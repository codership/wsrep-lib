//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include <wsrep_api.h> // Wsrep typedefs

// Forward declarations
namespace wsrep
{
    class client_context;
    class mock_server_context;
}

#include "wsrep/transaction_context.hpp"

//
// Utility functions
//
namespace wsrep_mock
{

    // Simple BF abort method to BF abort unordered transasctions
    void bf_abort_unordered(wsrep::client_context& cc);

    // BF abort method to abort transactions via provider
    void bf_abort_provider(wsrep::mock_server_context& sc,
                           const wsrep::transaction_context& tc,
                           wsrep_seqno_t bf_seqno);

    void start_applying_transaction(
        wsrep::client_context& cc,
        const wsrep::transaction_id& id,
        wsrep_seqno_t seqno,
        uint32_t flags);
}
