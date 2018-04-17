//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MOCK_SERVER_CONTEXT_HPP
#define TRREP_MOCK_SERVER_CONTEXT_HPP

#include "server_context.hpp"
#include "mock_provider_impl.hpp"

namespace trrep
{
    class mock_server_context : public trrep::server_context
    {
    public:
        mock_server_context(const std::string& name,
                            const std::string& id,
                            enum trrep::server_context::rollback_mode rollback_mode)
            : trrep::server_context(name, id, rollback_mode)
            , mock_provider_impl_()
            , provider_(&mock_provider_impl_)
            , last_client_id_(0)
        { }
        trrep::provider& provider() const
        { return provider_; }
        trrep::mock_provider_impl& mock_provider() const
        { return mock_provider_impl_; }
        trrep::client_context* local_client_context()
        {
            return new trrep::client_context(*this, ++last_client_id_,
                                             trrep::client_context::m_local);
        }

        void on_connect() { }
        void on_view() { }
        void on_sync() { }
        // void on_apply(trrep::transaction_context&) { }
        // void on_commit(trrep::transaction_context&) { }

    private:
        mutable trrep::mock_provider_impl mock_provider_impl_;
        mutable trrep::provider provider_;
        unsigned long long last_client_id_;
    };
}

#endif // TRREP_MOCK_SERVER_CONTEXT_HPP
