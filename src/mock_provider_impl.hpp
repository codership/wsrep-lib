//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_MOCK_PROVIDER_IMPL_HPP
#define TRREP_MOCK_PROVIDER_IMPL_HPP

#include "provider_impl.hpp"

#include <cstring>
#include <map>
#include <iostream> // todo: proper logging

namespace trrep
{
    class mock_provider_impl : public trrep::provider_impl
    {
    public:
        typedef std::map<std::pair<wsrep_conn_id_t, wsrep_trx_id_t>, wsrep_seqno_t > bf_abort_map;
        mock_provider_impl()
            : group_id_()
            , node_id_()
            , group_seqno_(0)
            , bf_abort_map_()
        {

            memset(&group_id_, 0, sizeof(group_id_));
            group_id_.data[0] = 1;

            memset(&node_id_, 0, sizeof(node_id_));
            node_id_.data[0] = 1;
        }
        // Provider implemenatation interface
        int start_transaction(wsrep_ws_handle_t*) { return 0; }
        wsrep_status
        certify(wsrep_conn_id_t conn_id,
                wsrep_ws_handle_t* ws_handle,
                uint32_t flags,
                wsrep_trx_meta_t* trx_meta)
        {
            assert(flags | WSREP_FLAG_TRX_END);
            if ((flags | WSREP_FLAG_TRX_END) == 0)
            {
                return WSREP_FATAL;
            }
            std::cerr << "certify_commit: " << conn_id << ":"
                      << ws_handle->trx_id << "\n";
            trx_meta->stid.node = node_id_;
            trx_meta->stid.trx = ws_handle->trx_id;
            trx_meta->stid.conn = conn_id;

            std::pair<wsrep_conn_id_t, wsrep_trx_id_t>
                trx_id(conn_id, ws_handle->trx_id);
            bf_abort_map::iterator it(bf_abort_map_.find(trx_id));
            std::cerr << "bf aborted: "
                      << (it == bf_abort_map_.end() ? "no" : "yes") << "\n";
            if (it == bf_abort_map_.end())
            {
                ++group_seqno_;
                trx_meta->gtid.uuid = group_id_;
                trx_meta->gtid.seqno = group_seqno_;
                trx_meta->depends_on = group_seqno_ - 1;
                std::cerr << "certify_commit return: " << WSREP_OK << "\n";
                return WSREP_OK;
            }
            else
            {
                wsrep_status_t ret;
                if (it->second == WSREP_SEQNO_UNDEFINED)
                {
                    trx_meta->gtid.uuid = WSREP_UUID_UNDEFINED;
                    trx_meta->gtid.seqno = WSREP_SEQNO_UNDEFINED;
                    trx_meta->depends_on = WSREP_SEQNO_UNDEFINED;
                    ret = WSREP_TRX_FAIL;
                }
                else
                {
                    ++group_seqno_;
                    trx_meta->gtid.uuid = group_id_;
                    trx_meta->gtid.seqno = group_seqno_;
                    trx_meta->depends_on = group_seqno_ - 1;
                    ret = WSREP_BF_ABORT;
                }
                bf_abort_map_.erase(it);
                std::cerr << "certify_commit return: " << ret << "\n";
                return ret;
            }

        }

        int append_key(wsrep_ws_handle_t*, const wsrep_key_t*) { return 0; }
        int append_data(wsrep_ws_handle_t*, const wsrep_buf_t*) { return 0; }
        int rollback(const wsrep_trx_id_t) { return 0; }
        wsrep_status_t commit_order_enter(wsrep_ws_handle_t*)
        { return WSREP_OK; }
        int commit_order_leave(wsrep_ws_handle_t*) { return 0;}
        int release(wsrep_ws_handle_t*) { return 0; }

        // Methods to modify mock state

        // Inject BF abort event into the provider.
        //
        // If bf_seqno equals to WSREP_SEQNO_UNDEFINED, the BF abort
        // is done without ordering and the provider will return
        // WSREP_TRX_FAIL to the aborted transaction. Otherwise
        // WSREP_BF_ABORT is returned to the aborted transaction.
        //
        // @param conn_id Connection/client identifier to be aborted
        // @param trx_id Trx id to be aborted
        // @param bf_seqno Aborter sequence number
        //
        void bf_abort(wsrep_conn_id_t conn_id,
                      wsrep_trx_id_t trx_id,
                      wsrep_seqno_t bf_seqno)
        {
            std::cerr << "bf_abort: " << conn_id << ":" << trx_id << "\n";
            assert(bf_seqno == WSREP_SEQNO_UNDEFINED ||
                   group_seqno_ < bf_seqno);
            bf_abort_map_.insert(std::make_pair(std::make_pair(conn_id, trx_id),
                                                bf_seqno));
            if (bf_seqno != WSREP_SEQNO_UNDEFINED)
            {
                group_seqno_ = bf_seqno;
            }
        }
    private:
        wsrep_uuid_t group_id_;
        wsrep_uuid_t node_id_;
        wsrep_seqno_t group_seqno_;
        bf_abort_map bf_abort_map_;
    };
}


#endif // TRREP_MOCK_PROVIDER_IMPL_HPP
