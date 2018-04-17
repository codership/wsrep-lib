//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "client_context.hpp"
#include "transaction_context.hpp"

trrep::provider& trrep::client_context::provider() const
{
    return server_context_.provider();
}

// TODO: This should be pure virtual method, implemented by
// DBMS integration or mock classes only.
int trrep::client_context::replay(trrep::transaction_context& tc)
{
    trrep::unique_lock<trrep::mutex> lock(mutex_);
    tc.state(lock, trrep::transaction_context::s_replaying);
    tc.state(lock, trrep::transaction_context::s_committed);
    return 0;
}
