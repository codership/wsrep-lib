//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DB_STORAGE_ENGINE_HPP
#define WSREP_DB_STORAGE_ENGINE_HPP

#include "db_params.hpp"

#include "wsrep/mutex.hpp"
#include "wsrep/client_context.hpp"

#include <atomic>
#include <unordered_set>

namespace db
{
    class client;
    class storage_engine
    {
    public:
        storage_engine(const params& params)
            : mutex_()
            , transactions_()
            , alg_freq_(params.alg_freq)
            , bf_aborts_()
        { }

        class transaction
        {
        public:
            transaction(storage_engine& se)
                : se_(se)
                , cc_()
            { }
            ~transaction()
            {
                rollback();
            }
            bool active() const { return cc_ != nullptr; }
            void start(client* cc);
            void commit();
            void rollback();
            transaction(const transaction&) = delete;
            transaction& operator=(const transaction&) = delete;
        private:
            storage_engine& se_;
            client* cc_;
        };
        void bf_abort_some(const wsrep::transaction_context& tc);
        long long bf_aborts() const { return bf_aborts_; }
    private:
        wsrep::default_mutex mutex_;
        std::unordered_set<db::client*> transactions_;
        size_t alg_freq_;
        std::atomic<long long> bf_aborts_;
    };
}

#endif // WSREP_DB_STORAGE_ENGINE_HPP
