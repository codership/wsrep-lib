//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

/** @file high_priority_service.hpp
 *
 * Interface for services for applying high priority transactions.
 */
#ifndef WSREP_HIGH_PRIORITY_SERVICE_HPP
#define WSREP_HIGH_PRIORITY_SERVICE_HPP

#include "server_state.hpp"

namespace wsrep
{
    class ws_handle;
    class ws_meta;
    class const_buffer;
    class transaction;
    class high_priority_service
    {
    public:
        high_priority_service(wsrep::server_state& server_state)
            : server_state_(server_state)
            , must_exit_() { }
        virtual ~high_priority_service() { }

        int apply(const ws_handle& ws_handle, const ws_meta& ws_meta,
                          const const_buffer& data)
        {
            return server_state_.on_apply(*this, ws_handle, ws_meta, data);
        }
        /**
         * Start a new transaction
         */
        virtual int start_transaction(const wsrep::ws_handle&,
                                      const wsrep::ws_meta&) = 0;

        /**
         * Adopt a transaction.
         */
        virtual void adopt_transaction(const wsrep::transaction&) = 0;

        /**
         * Apply a write set.
         *
         * A write set applying happens always
         * as a part of the transaction. The caller must start a
         * new transaction before applying a write set and must
         * either commit to make changes persistent or roll back.
         */
        virtual int apply_write_set(const wsrep::const_buffer&) = 0;

        /**
         *
         */
        virtual int append_fragment(const wsrep::ws_meta&,
                                    const wsrep::const_buffer& data) = 0;
        /**
         * Commit a transaction.
         */
        virtual int commit() = 0;

        /**
         * Roll back a transaction
         */
        virtual int rollback() = 0;

        /**
         * Apply a TOI operation.
         *
         * TOI operation is a standalone operation and should not
         * be executed as a part of a transaction.
         */
        virtual int apply_toi(const wsrep::ws_meta&,
                              const wsrep::const_buffer&) = 0;

        /**
         * Actions to take after applying a write set was completed.
         */
        virtual void after_apply() = 0;

        virtual void store_globals() = 0;
        virtual void reset_globals() = 0;

        virtual int log_dummy_write_set(const ws_handle&, const ws_meta&) = 0;

        virtual bool is_replaying() const = 0;

        bool must_exit() const { return must_exit_; }
    protected:
        wsrep::server_state& server_state_;
        bool must_exit_;
    };

    class high_priority_switch
    {
    public:
        high_priority_switch(high_priority_service& orig_service,
                             high_priority_service& current_service)
            : orig_service_(orig_service)
            , current_service_(current_service)
        {
            orig_service_.reset_globals();
            current_service_.store_globals();
        }
        ~high_priority_switch()
        {
            current_service_.reset_globals();
            orig_service_.store_globals();
        }
    private:
        high_priority_service& orig_service_;
        high_priority_service& current_service_;
    };
}

#endif // WSREP_HIGH_PRIORITY_SERVICE_HPP
