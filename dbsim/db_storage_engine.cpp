//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "db_storage_engine.hpp"
#include "db_client.hpp"

void db::storage_engine::transaction::start(db::client* cc)
{
    wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
    if (se_.transactions_.insert(cc).second == false)
    {
        ::abort();
    }
    cc_ = cc;
}

void db::storage_engine::transaction::apply(
    const wsrep::transaction& transaction)
{
    assert(cc_);
    se_.bf_abort_some(transaction);
}

void db::storage_engine::transaction::commit()
{
    if (cc_)
    {
        wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
        se_.transactions_.erase(cc_);
    }
    cc_ = nullptr;
}


void db::storage_engine::transaction::rollback()
{
    if (cc_)
    {
        wsrep::unique_lock<wsrep::mutex> lock(se_.mutex_);
        se_.transactions_.erase(cc_);
    }
    cc_ = nullptr;
}

void db::storage_engine::bf_abort_some(const wsrep::transaction& txc)
{
    wsrep::unique_lock<wsrep::mutex> lock(mutex_);
    if (alg_freq_ && (std::rand() % alg_freq_) == 0)
    {
        if (transactions_.empty() == false)
        {
            for (auto victim : transactions_)
            {
                wsrep::client_state& cc(victim->client_state());
                if (cc.mode() == wsrep::client_state::m_local)
                {
                    if (victim->bf_abort(txc.seqno()))
                    {
                        ++bf_aborts_;
                    }
                    break;
                }
            }
        }
    }
}
