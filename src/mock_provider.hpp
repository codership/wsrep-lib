//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_PROVIDER_HPP
#define WSREP_MOCK_PROVIDER_HPP

#include "wsrep/provider.hpp"

#include <cstring>
#include <map>
#include <iostream> // todo: proper logging

namespace wsrep
{
    class mock_provider : public wsrep::provider
    {
    public:
        typedef std::map<wsrep::transaction_id, wsrep::seqno> bf_abort_map;

        mock_provider(wsrep::server_context& server_context)
            : provider(server_context)
            , group_id_("1")
            , server_id_("1")
            , group_seqno_(0)
            , bf_abort_map_()
            , next_error_(wsrep::provider::success)
        { }

        int connect(const std::string&, const std::string&, const std::string&,
                    bool)
        { return 0; }
        int disconnect() { return 0; }
        enum wsrep::provider::status run_applier(void*)
        {
            return wsrep::provider::success;
        }
        // Provider implemenatation interface
        int start_transaction(wsrep::ws_handle&) { return 0; }
        enum wsrep::provider::status
        certify(wsrep::client_id client_id,
                wsrep::ws_handle& ws_handle,
                int flags,
                wsrep::ws_meta& ws_meta)
        {
            assert(flags & wsrep::provider::flag::start_transaction);
            if (next_error_)
            {
                return next_error_;
            }
            if ((flags & wsrep::provider::flag::commit) == 0)
            {
                return wsrep::provider::error_provider_failed;
            }
            wsrep::stid stid(server_id_,
                             ws_handle.transaction_id(),
                             client_id);
            bf_abort_map::iterator it(bf_abort_map_.find(
                                          ws_handle.transaction_id()));
            if (it == bf_abort_map_.end())
            {
                ++group_seqno_;
                wsrep::gtid gtid(group_id_, wsrep::seqno(group_seqno_));
                ws_meta = wsrep::ws_meta(gtid, stid,
                                         wsrep::seqno(group_seqno_ - 1),
                                         flags);
                return wsrep::provider::success;
            }
            else
            {
                enum wsrep::provider::status ret;
                if (it->second.nil())
                {
                    ws_meta = wsrep::ws_meta(wsrep::gtid(), wsrep::stid(),
                                             wsrep::seqno::undefined(), 0);
                    ret = wsrep::provider::error_certification_failed;
                }
                else
                {
                    ++group_seqno_;
                    wsrep::gtid gtid(group_id_, wsrep::seqno(group_seqno_));
                    ws_meta = wsrep::ws_meta(gtid, stid,
                                             wsrep::seqno(group_seqno_ - 1),
                                             flags);
                    ret = wsrep::provider::error_bf_abort;
                }
                bf_abort_map_.erase(it);
                return ret;
            }
        }

        int append_key(wsrep::ws_handle&, const wsrep::key&)
        { return next_error_; }
        int append_data(wsrep::ws_handle&, const wsrep::data&)
        { return next_error_; }
        int rollback(const wsrep::transaction_id)
        { return next_error_; }
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle&,
                                          const wsrep::ws_meta&)
        { return next_error_; }
        int commit_order_leave(const wsrep::ws_handle&,
                               const wsrep::ws_meta&)
        { return next_error_;}
        int release(wsrep::ws_handle&)
        { return next_error_; }

        int replay(wsrep::ws_handle&, void*) { ::abort(); /* not impl */}

        int sst_sent(const wsrep::gtid&, int) { return 0; }
        int sst_received(const wsrep::gtid&, int) { return 0; }

        std::vector<status_variable> status() const
        {
            return std::vector<status_variable>();
        }

        // Methods to modify mock state
        /*! Inject BF abort event into the provider.
         *
         * \param bf_seqno Aborter sequence number
         * \param trx_id Trx id to be aborted
         * \param[out] victim_seqno
         */
        enum wsrep::provider::status
        bf_abort(wsrep::seqno bf_seqno,
                 wsrep::transaction_id trx_id,
                 wsrep::seqno& victim_seqno)
        {
            std::cerr << "bf_abort: " << trx_id << "\n";
            bf_abort_map_.insert(std::make_pair(trx_id, bf_seqno));
            if (bf_seqno.nil() == false)
            {
                group_seqno_ = bf_seqno.get();
            }
            victim_seqno = wsrep::seqno::undefined();
            return wsrep::provider::success;
        }

        /*!
         * \todo Inject an error so that the next call to any
         *       provider call will return the given error.
         */
        void inject_error(enum wsrep::provider::status error)
        {
            next_error_ = error;
        }
    private:
        wsrep::id group_id_;
        wsrep::id server_id_;
        long long group_seqno_;
        bf_abort_map bf_abort_map_;
        enum wsrep::provider::status next_error_;
    };
}


#endif // WSREP_MOCK_PROVIDER_HPP
