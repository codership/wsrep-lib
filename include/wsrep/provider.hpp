//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_PROVIDER_HPP
#define WSREP_PROVIDER_HPP

#include "key.hpp"
#include "data.hpp"
#include "client_id.hpp"
#include "transaction_id.hpp"

#include <cassert>
#include <cstring>

#include <string>
#include <vector>
#include <ostream>

namespace wsrep
{
    /*! \class seqno
     *
     * Sequence number type.
     *
     * By convention, nil value is zero, negative values are not allowed.
     * Relation operators are restricted to < and > on purpose to
     * enforce correct use.
     */
    class seqno
    {
    public:
        seqno()
            : seqno_()
        { }

        template <typename I>
        seqno(I seqno)
            : seqno_(seqno)
        {
            assert(seqno_ >= 0);
        }

        long long get() const
        {
            return seqno_;
        }

        bool nil() const
        {
            return (seqno_ == 0);
        }

        bool operator<(seqno other) const
        {
            return (seqno_ < other.seqno_);
        }

        bool operator>(seqno other) const
        {
            return (seqno_ > other.seqno_);
        }

        seqno operator+(seqno other) const
        {
            return (seqno_ + other.seqno_);
        }

        static seqno undefined() { return 0; }
    private:
        long long seqno_;
    };

    static inline std::ostream& operator<<(std::ostream& os, wsrep::seqno seqno)
    {
        return (os << seqno.get());
    }

    class id
    {
    public:
        enum type
        {
            none,
            string,
            uuid
        };

        id()
            : type_(none)
            , data_()
        { }

        id(const std::string&);

        id (const void* data, size_t data_size)
            : type_()
            , data_()
        {
            assert(data_size <= 16);
            std::memcpy(data_, data, data_size);
        }
#if 0
        void assign(const void* data, size_t data_size)
        {
            assert(data_size == 16);
            memcpy(data_, data, data_size);
        }
#endif
        bool operator<(const id& other) const
        {
            return (std::memcmp(data_, other.data_, sizeof(data_)) < 0);
        }
        bool operator==(const id& other) const
        {
            return (std::memcmp(data_, other.data_, sizeof(data_)) == 0);
        }
        const unsigned char* data() const { return data_; }
        size_t data_size() const { return 16; }

    private:
        enum type type_;
        unsigned char data_[16];
    };

    class gtid
    {
    public:
        gtid()
            : id_()
            , seqno_()
        { }
        gtid(const wsrep::id& id, wsrep::seqno seqno)
            : id_(id)
            , seqno_(seqno)
        { }
        const wsrep::id& id() const { return id_; }
        wsrep::seqno seqno() const { return seqno_ ; }
    private:
        wsrep::id id_;
        wsrep::seqno seqno_;
    };


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

        /*!
         * Return value enumeration
         *
         * \todo Convert this to struct ec, get rid of prefixes.
         */
        enum status
        {
            /*! Success */
            success,
            /*! Warning*/
            error_warning,
            /*! Transaction was not found from provider */
            error_transaction_missing,
            /*! Certification failed */
            error_certification_failed,
            /*! Transaction was BF aborted */
            error_bf_abort,
            /*! Transaction size exceeded */
            error_size_exceeded,
            /*! Connectivity to cluster lost */
            error_connection_failed,
            /*! Internal provider failure, provider must be reinitialized */
            error_provider_failed,
            /*! Fatal error, server must abort */
            error_fatal,
            /*! Requested functionality is not implemented by the provider */
            error_not_implemented,
            /*! Operation is not allowed */
            error_not_allowed,
            /*! Unknown error code from the provider */
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
        virtual int append_data(wsrep::ws_handle&, const wsrep::data&) = 0;
        virtual enum status
        certify(wsrep::client_id, wsrep::ws_handle&,
                int,
                wsrep::ws_meta&) = 0;
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
        virtual enum status bf_abort(wsrep::seqno bf_seqno,
                                     wsrep::transaction_id victim_trx,
                                     wsrep::seqno& victim_seqno) = 0;
        virtual int rollback(wsrep::transaction_id) = 0;
        virtual enum status commit_order_enter(const wsrep::ws_handle&,
                                               const wsrep::ws_meta&) = 0;
        virtual int commit_order_leave(const wsrep::ws_handle&,
                                       const wsrep::ws_meta&) = 0;
        virtual int release(wsrep::ws_handle&) = 0;

        /*!
         * Replay a transaction.
         *
         * @return Zero in case of success, non-zero on failure.
         */
        virtual int replay(wsrep::ws_handle& ws_handle, void* applier_ctx) = 0;

        virtual int sst_sent(const wsrep::gtid&, int) = 0;
        virtual int sst_received(const wsrep::gtid&, int) = 0;

        virtual std::vector<status_variable> status() const = 0;

        /*!
         * Return a pointer to native handle.
         *
         * \todo This should be eventually deprecated.
         */
        // virtual struct wsrep* native() = 0;
        // Factory method
        static provider* make_provider(const std::string& provider);
    };
}

#endif // WSREP_PROVIDER_HPP
