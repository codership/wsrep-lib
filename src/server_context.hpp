//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_SERVER_CONTEXT_HPP
#define TRREP_SERVER_CONTEXT_HPP

#include "exception.hpp"

#include "wsrep_api.h"

#include <string>

namespace trrep
{
    // Forward declarations
    class provider;
    class client_context;
    class transaction_context;
    class data;

    class server_context
    {
    public:

        enum rollback_mode
        {
            rm_async,
            rm_sync
        };

        server_context(const std::string& name,
                       const std::string& id,
                       enum rollback_mode rollback_mode)
            : provider_()
            , name_(name)
            , id_(id)
            , rollback_mode_(rollback_mode)

        { }

        //
        // Return server name
        //
        const std::string& name() const { return name_; }

        //
        // Return server identifier
        //
        const std::string& id() const { return id_; }

        //
        // Create client context which acts only locally, i.e. does
        // not participate in replication. However, local client
        // connection may execute transactions which require ordering,
        // as when modifying local SR fragment storage requires
        // strict commit ordering.
        //
        virtual client_context* local_client_context() = 0;

        //
        // Load provider
        //
        // @return Zero on success, non-zero on error
        //
        int load_provider(const std::string&);

        //
        // Return reference to provider
        //
        // @return Reference to provider
        // @throw trrep::runtime_error if provider has not been loaded
        //
        virtual trrep::provider& provider() const
        {
            if (provider_ == 0)
            {
                throw trrep::runtime_error("provider not loaded");
            }
            return *provider_;
        }

        //
        //
        //
        virtual void on_connect() = 0;
        virtual void on_view() = 0;
        virtual void on_sync() = 0;

        //
        // This method will be called by the applier thread when
        // a remote write set is being applied. It is the responsibility
        // of the caller to set up transaction context and data properly.
        //
        int on_apply(trrep::client_context& client_context,
                     trrep::transaction_context& transaction_context,
                     const trrep::data& data);

        virtual bool statement_allowed_for_streaming(
            const trrep::client_context&,
            const trrep::transaction_context&) const
        {
            // Streaming not implemented yet
            return false;
        }
    private:

        server_context(const server_context&);
        server_context& operator=(const server_context&);

        trrep::provider* provider_;
        std::string name_;
        std::string id_;
        enum rollback_mode rollback_mode_;
    };
}

#endif // TRREP_SERVER_CONTEXT_HPP
