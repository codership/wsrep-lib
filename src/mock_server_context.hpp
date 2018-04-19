//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MOCK_SERVER_CONTEXT_HPP
#define TRREP_MOCK_SERVER_CONTEXT_HPP

#include "server_context.hpp"
#include "mock_client_context.hpp"
#include "mock_provider.hpp"


namespace trrep
{
    class mock_server_context : public trrep::server_context
    {
    public:
        mock_server_context(const std::string& name,
                            const std::string& id,
                            enum trrep::server_context::rollback_mode rollback_mode)
            : trrep::server_context(name, id, rollback_mode)
            , provider_()
            , last_client_id_(0)
        { }
        trrep::mock_provider& provider() const
        { return provider_; }
        trrep::client_context* local_client_context()
        {
            return new trrep::mock_client_context(*this, ++last_client_id_,
                                                  trrep::client_context::m_local);
        }

        void on_connect() { }
        void wait_until_connected() { }
        void on_view(const trrep::view&) { }
        void on_sync() { }
        // void on_apply(trrep::transaction_context&) { }
        // void on_commit(trrep::transaction_context&) { }

    private:
        mutable trrep::mock_provider provider_;
        unsigned long long last_client_id_;
    };
}

#endif // TRREP_MOCK_SERVER_CONTEXT_HPP
