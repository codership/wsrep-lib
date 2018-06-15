//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_SERVER_CONTEXT_HPP
#define WSREP_DB_SERVER_CONTEXT_HPP

#include "wsrep/server_context.hpp"

#include <atomic>

namespace db
{
    class server_context : public wsrep::server_context
    {
    public:
            size_t next_transaction_id()
        {
            return (last_transaction_id_.fetch_add(1) + 1);
        }
    private:
        std::atomic<size_t> last_transaction_id_;
    };
}

#endif // WSREP_DB_SERVER_CONTEXT_HPP
