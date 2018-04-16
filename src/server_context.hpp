//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_SERVER_CONTEXT_HPP
#define TRREP_SERVER_CONTEXT_HPP

#include <string>

namespace trrep
{
    // Forward declarations
    class transaction_context;

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
            : name_(name)
            , id_(id)
            , rollback_mode_(rollback_mode)
            { }
        const std::string& name() const { return name_; }
        const std::string& id() const { return id_; }

        virtual void on_connect() { }
        virtual void on_view() { }
        virtual void on_sync() { }
        virtual void on_apply(trrep::transaction_context&) { }
        virtual void on_commit(trrep::transaction_context&) { }


        virtual bool statement_allowed_for_streaming(
            const trrep::client_context&,
            const trrep::transaction_context&) const
        {
            // Streaming not implemented yet
            return false;
        }
    private:
        std::string name_;
        std::string id_;
        enum rollback_mode rollback_mode_;
    };
}

#endif // TRREP_SERVER_CONTEXT_HPP
