//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_PROVIDER_HPP
#define TRREP_PROVIDER_HPP

// #include "provider_impl.hpp"

#include <wsrep_api.h>

#include <string>
#include <vector>

namespace trrep
{
    // Abstract interface for provider implementations
    class provider
    {
    public:

        class status_variable
        {
        public:
            status_variable(const std::string& name,
                            const std::string& value)
                : name_(name)
                , value_(value)
            { }
            const std::string& name() const { return name_; }
            const std::string& value() const { return value_; }
        private:
            std::string name_;
            std::string value_;
        };
        virtual ~provider() { }
        // Provider state management
        virtual int connect(const std::string& cluster_name,
                            const std::string& cluster_url,
                            const std::string& state_donor,
                            bool bootstrap) = 0;
        virtual int disconnect() = 0;


        // Applier interface
        virtual wsrep_status_t run_applier(void* applier_ctx) = 0;
        // Write set replication
        // TODO: Rename to assing_read_view()
        virtual int start_transaction(wsrep_ws_handle_t*) = 0;
        virtual int append_key(wsrep_ws_handle_t*, const wsrep_key_t*) = 0;
        virtual int append_data(wsrep_ws_handle_t*, const wsrep_buf_t*) = 0;
        virtual wsrep_status_t
        certify(wsrep_conn_id_t, wsrep_ws_handle_t*,
                uint32_t,
                wsrep_trx_meta_t*) = 0;
        /*!
         * BF abort a transaction inside provider.
         *
         * @param[in] bf_seqno Seqno of the aborter transaction
         * @param[in] victim_txt Transaction identifier of the victim
         * @param[out] victim_seqno Sequence number of the victim transaction
         * or WSREP_SEQNO_UNDEFINED if the victim was not ordered
         *
         * @return wsrep_status_t
         */
        virtual wsrep_status_t bf_abort(wsrep_seqno_t bf_seqno,
                                        wsrep_trx_id_t victim_trx,
                                        wsrep_seqno_t* victim_seqno) = 0;
        virtual int rollback(const wsrep_trx_id_t) = 0;
        virtual wsrep_status commit_order_enter(const wsrep_ws_handle_t*,
                                                const wsrep_trx_meta_t*) = 0;
        virtual int commit_order_leave(const wsrep_ws_handle_t*,
                                       const wsrep_trx_meta_t*) = 0;
        virtual int release(wsrep_ws_handle_t*) = 0;

        /*!
         * Replay a transaction.
         *
         * @return Zero in case of success, non-zero on failure.
         */
        virtual int replay(wsrep_ws_handle_t* ws_handle, void* applier_ctx) = 0;

        virtual int sst_sent(const wsrep_gtid_t&, int) = 0;
        virtual int sst_received(const wsrep_gtid_t&, int) = 0;

        virtual std::vector<status_variable> status() const = 0;
        // Factory method
        static provider* make_provider(const std::string& provider);
    };
}

#endif // TRREP_PROVIDER_HPP
