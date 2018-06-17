//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_PROVIDER_HPP
#define WSREP_PROVIDER_HPP

#include "gtid.hpp"
#include "key.hpp"
#include "buffer.hpp"
#include "client_id.hpp"
#include "transaction_id.hpp"

#include <cassert>
#include <cstring>

#include <string>
#include <vector>

namespace wsrep
{
    class server_state;

    class stid
    {
    public:
        stid()
            : server_id_()
            , transaction_id_()
            , client_id_()
        { }
        stid(const wsrep::id& server_id,
             wsrep::transaction_id transaction_id,
             wsrep::client_id client_id)
            : server_id_(server_id)
            , transaction_id_(transaction_id)
            , client_id_(client_id)
        { }
        const wsrep::id& server_id() const { return server_id_; }
        wsrep::transaction_id transaction_id() const
        { return transaction_id_; }
        wsrep::client_id client_id() const { return client_id_; }
    private:
        wsrep::id server_id_;
        wsrep::transaction_id transaction_id_;
        wsrep::client_id client_id_;
    };

    class ws_handle
    {
    public:
        ws_handle()
            : transaction_id_()
            , opaque_()
        { }
        ws_handle(wsrep::transaction_id id)
            : transaction_id_(id)
            , opaque_()
        { }
        ws_handle(wsrep::transaction_id id,
                  void* opaque)
            : transaction_id_(id)
            , opaque_(opaque)
        { }

        wsrep::transaction_id transaction_id() const
        { return transaction_id_; }

        void* opaque() const { return opaque_; }

    private:
        wsrep::transaction_id transaction_id_;
        void* opaque_;
    };

    class ws_meta
    {
    public:
        ws_meta()
            : gtid_()
            , stid_()
            , depends_on_()
            , flags_()
        { }
        ws_meta(const wsrep::gtid& gtid,
                const wsrep::stid& stid,
                wsrep::seqno depends_on,
                int flags)
            : gtid_(gtid)
            , stid_(stid)
            , depends_on_(depends_on)
            , flags_(flags)
        { }

        const wsrep::id& group_id() const
        {
            return gtid_.id();
        }

        wsrep::seqno seqno() const
        {
            return gtid_.seqno();
        }

        const wsrep::id& server_id() const
        {
            return stid_.server_id();
        }

        wsrep::client_id client_id() const
        {
            return stid_.client_id();
        }

        wsrep::transaction_id transaction_id() const
        {
            return stid_.transaction_id();
        }

        wsrep::seqno depends_on() const { return depends_on_; }

        int flags() const { return flags_; }
    private:
        wsrep::gtid gtid_;
        wsrep::stid stid_;
        wsrep::seqno depends_on_;
        int flags_;
    };


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

        /**
         * Return value enumeration
         *
         * @todo Convert this to struct ec, get rid of prefixes.
         */
        enum status
        {
            /** Success */
            success,
            /** Warning*/
            error_warning,
            /** Transaction was not found from provider */
            error_transaction_missing,
            /** Certification failed */
            error_certification_failed,
            /** Transaction was BF aborted */
            error_bf_abort,
            /** Transaction size exceeded */
            error_size_exceeded,
            /** Connectivity to cluster lost */
            error_connection_failed,
            /** Internal provider failure, provider must be reinitialized */
            error_provider_failed,
            /** Fatal error, server must abort */
            error_fatal,
            /** Requested functionality is not implemented by the provider */
            error_not_implemented,
            /** Operation is not allowed */
            error_not_allowed,
            /** Unknown error code from the provider */
            error_unknown
        };

        struct flag
        {
            static const int start_transaction = (1 << 0);
            static const int commit = (1 << 1);
            static const int rollback = (1 << 2);
            static const int isolation = (1 << 3);
            static const int pa_unsafe = (1 << 4);
            static const int commutative = (1 << 5);
            static const int native = (1 << 6);
            static const int snapshot = (1 << 7);
        };

        provider(wsrep::server_state& server_state)
            : server_state_(server_state)
        { }
        virtual ~provider() { }
        // Provider state management
        virtual int connect(const std::string& cluster_name,
                            const std::string& cluster_url,
                            const std::string& state_donor,
                            bool bootstrap) = 0;
        virtual int disconnect() = 0;


        // Applier interface
        virtual enum status run_applier(void* applier_ctx) = 0;
        // Write set replication
        // TODO: Rename to assing_read_view()
        virtual int start_transaction(wsrep::ws_handle&) = 0;
        virtual int append_key(wsrep::ws_handle&, const wsrep::key&) = 0;
        virtual int append_data(wsrep::ws_handle&, const wsrep::const_buffer&) = 0;
        virtual enum status
        certify(wsrep::client_id, wsrep::ws_handle&,
                int,
                wsrep::ws_meta&) = 0;
        /**
         * BF abort a transaction inside provider.
         *
         * @param[in] bf_seqno Seqno of the aborter transaction
         * @param[in] victim_txt Transaction identifier of the victim
         * @param[out] victim_seqno Sequence number of the victim transaction
         * or WSREP_SEQNO_UNDEFINED if the victim was not ordered
         *
         * @return wsrep_status_t
         */
        virtual enum status bf_abort(wsrep::seqno bf_seqno,
                                     wsrep::transaction_id victim_trx,
                                     wsrep::seqno& victim_seqno) = 0;
        virtual int rollback(wsrep::transaction_id) = 0;
        virtual enum status commit_order_enter(const wsrep::ws_handle&,
                                               const wsrep::ws_meta&) = 0;
        virtual int commit_order_leave(const wsrep::ws_handle&,
                                       const wsrep::ws_meta&) = 0;
        virtual int release(wsrep::ws_handle&) = 0;

        /**
         * Replay a transaction.
         *
         * @todo Inspect if the ws_handle could be made const
         *
         * @return Zero in case of success, non-zero on failure.
         */
        virtual enum status replay(
            wsrep::ws_handle& ws_handle, void* applier_ctx) = 0;

        virtual int sst_sent(const wsrep::gtid&, int) = 0;
        virtual int sst_received(const wsrep::gtid&, int) = 0;

        virtual std::vector<status_variable> status() const = 0;

        /**
         * Create a new provider.
         *
         * @param provider_spec Provider specification
         * @param provider_options Initial options to provider
         */
        static provider* make_provider(
            wsrep::server_state&,
            const std::string& provider_spec,
            const std::string& provider_options);
    protected:
        wsrep::server_state& server_state_;
    };

    std::string flags_to_string(int flags);

    static inline bool starts_transaction(int flags)
    {
        return (flags & wsrep::provider::flag::start_transaction);
    }

    static inline bool commits_transaction(int flags)
    {
        return (flags & wsrep::provider::flag::commit);
    }

    static inline bool rolls_back_transaction(int flags)
    {
        return (flags & wsrep::provider::flag::rollback);
    }


}

#endif // WSREP_PROVIDER_HPP
