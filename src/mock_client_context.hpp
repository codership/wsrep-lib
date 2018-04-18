//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MOCK_CLIENT_CONTEXT_HPP
#define TRREP_MOCK_CLIENT_CONTEXT_HPP

#include "client_context.hpp"
#include "mutex.hpp"

namespace trrep
{
    class mock_client_context : public trrep::client_context
    {
    public:
        mock_client_context(trrep::server_context& server_context,
                            const trrep::client_id& id,
                            enum trrep::client_context::mode mode)
            : trrep::client_context(mutex_, server_context, id, mode)
              // Note: Mutex is initialized only after passed
              // to client_context constructor.
            , mutex_()
        { }
    private:
        trrep::default_mutex mutex_;
    };
}

#endif // TRREP_MOCK_CLIENT_CONTEXT_HPP
