//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_PROVIDER_HPP
#define WSREP_MOCK_PROVIDER_HPP

#include "wsrep/provider.hpp"
#include "wsrep/logger.hpp"
#include "wsrep/buffer.hpp"
#include "wsrep/high_priority_service.hpp"

#include <cstring>
#include <map>
#include <iostream> // todo: proper logging

#include <boost/test/unit_test.hpp>

namespace wsrep
{
    class mock_provider : public wsrep::provider
    {
    public:
        typedef std::map<wsrep::transaction_id, wsrep::seqno> bf_abort_map;

        mock_provider(wsrep::server_state& server_state)
            : provider(server_state)
            , certify_result_()
            , commit_order_enter_result_()
            , commit_order_leave_result_()
            , release_result_()
            , replay_result_()
            , group_id_("1")
            , server_id_("1")
            , group_seqno_(0)
            , bf_abort_map_()
            , start_fragments_()
            , fragments_()
            , commit_fragments_()
            , rollback_fragments_()
        { }

        enum wsrep::provider::status
        connect(const std::string&, const std::string&, const std::string&,
                bool)
        { return wsrep::provider::success; }
        int disconnect() { return 0; }
        int capabilities() const { return 0; }
        int desync() { return 0; }
        int resync() { return 0; }
        wsrep::seqno pause() { return wsrep::seqno(0); }
        int resume() { return 0; }
        enum wsrep::provider::status run_applier(wsrep::high_priority_service*)
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
            ws_handle = wsrep::ws_handle(ws_handle.transaction_id(), (void*)1);
            wsrep::log_info() << "provider certify: "
                              << "client: " << client_id.get()
                              << " flags: " << std::hex << flags
                              << std::dec
                              << " certify_status: " << certify_result_;

            if (certify_result_)
            {
                return certify_result_;
            }

            ++fragments_;
            if (starts_transaction(flags))
            {
                ++start_fragments_;
            }
            if (commits_transaction(flags))
            {
                ++commit_fragments_;
            }
            if (rolls_back_transaction(flags))
            {
                assert(0);
                ++rollback_fragments_;
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
                if (it->second.is_undefined())
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
        { return 0; }
        enum wsrep::provider::status
        append_data(wsrep::ws_handle&, const wsrep::const_buffer&)
        { return wsrep::provider::success; }
        enum wsrep::provider::status rollback(const wsrep::transaction_id)
        {
            ++fragments_;
            ++rollback_fragments_;
            return wsrep::provider::success;
        }
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle& ws_handle,
                           const wsrep::ws_meta& ws_meta)
        {
            BOOST_REQUIRE(ws_handle.opaque());
            BOOST_REQUIRE(ws_meta.seqno().is_undefined() == false);
            return commit_order_enter_result_;
        }

        int commit_order_leave(const wsrep::ws_handle& ws_handle,
                               const wsrep::ws_meta& ws_meta)
        {
            BOOST_REQUIRE(ws_handle.opaque());
            BOOST_REQUIRE(ws_meta.seqno().is_undefined() == false);
            return commit_order_leave_result_;
        }

        int release(wsrep::ws_handle& )
        {
            // BOOST_REQUIRE(ws_handle.opaque());
            return release_result_;
        }

        enum wsrep::provider::status replay(
            const wsrep::ws_handle& ws_handle,
            wsrep::high_priority_service* hps)
        {
            wsrep::mock_high_priority_service& high_priority_service(
                *static_cast<wsrep::mock_high_priority_service*>(hps));
            wsrep::mock_client_state& cc(
                *high_priority_service.client_state());
            wsrep::high_priority_context high_priority_context(cc);
            const wsrep::transaction& tc(cc.transaction());
            wsrep::ws_meta ws_meta;
            if (replay_result_ == wsrep::provider::success)
            {
                // If the ws_meta was not assigned yet, the certify
                // returned early due to BF abort.
                if (tc.ws_meta().seqno().is_undefined())
                {
                    ++group_seqno_;
                    ws_meta = wsrep::ws_meta(
                        wsrep::gtid(group_id_, wsrep::seqno(group_seqno_)),
                        wsrep::stid(server_id_, tc.id(), cc.id()),
                        wsrep::seqno(group_seqno_ - 1),
                        wsrep::provider::flag::start_transaction |
                        wsrep::provider::flag::commit);
                }
                else
                {
                    ws_meta = tc.ws_meta();
                }
            }
            else
            {
                return replay_result_;
            }

            if (high_priority_service.apply(ws_handle, ws_meta,
                                            wsrep::const_buffer()))
            {
                return wsrep::provider::error_fatal;
            }
            return wsrep::provider::success;
        }

        enum wsrep::provider::status enter_toi(wsrep::client_id,
                                               const wsrep::key_array&,
                                               const wsrep::const_buffer&,
                                               wsrep::ws_meta&,
                                               int)
        { return wsrep::provider::success; }
        enum wsrep::provider::status leave_toi(wsrep::client_id)
        { return wsrep::provider::success; }

        std::pair<wsrep::gtid, enum wsrep::provider::status>
        causal_read(int) const WSREP_OVERRIDE
        {
            return std::make_pair(wsrep::gtid::undefined(),
                                  wsrep::provider::error_not_implemented);
        }
        enum wsrep::provider::status wait_for_gtid(const wsrep::gtid&,
            int) const WSREP_OVERRIDE
        { return wsrep::provider::success; }
        wsrep::gtid last_committed_gtid() const { return wsrep::gtid(); }
        int sst_sent(const wsrep::gtid&, int) { return 0; }
        int sst_received(const wsrep::gtid&, int) { return 0; }

        std::vector<status_variable> status() const
        {
            return std::vector<status_variable>();
        }
        void reset_status() { }
        std::string options() const { return ""; }
        enum wsrep::provider::status options(const std::string&)
        { return wsrep::provider::success; }
        void* native() const { return 0; }

        //
        // Methods to modify mock state
        //
        /** Inject BF abort event into the provider.
         *
         * @param bf_seqno Aborter sequence number
         * @param trx_id Trx id to be aborted
         * @param[out] victim_seqno
         */
        enum wsrep::provider::status
        bf_abort(wsrep::seqno bf_seqno,
                 wsrep::transaction_id trx_id,
                 wsrep::seqno& victim_seqno)
        {
            bf_abort_map_.insert(std::make_pair(trx_id, bf_seqno));
            if (bf_seqno.is_undefined() == false)
            {
                group_seqno_ = bf_seqno.get();
            }
            victim_seqno = wsrep::seqno::undefined();
            return wsrep::provider::success;
        }

        // Parameters to control return value from the call
        enum wsrep::provider::status certify_result_;
        enum wsrep::provider::status commit_order_enter_result_;
        enum wsrep::provider::status commit_order_leave_result_;
        enum wsrep::provider::status release_result_;
        enum wsrep::provider::status replay_result_;

        size_t start_fragments() const { return start_fragments_; }
        size_t fragments() const { return fragments_; }
        size_t commit_fragments() const { return commit_fragments_; }
        size_t rollback_fragments() const { return rollback_fragments_; }

    private:
        wsrep::id group_id_;
        wsrep::id server_id_;
        long long group_seqno_;
        bf_abort_map bf_abort_map_;
        size_t start_fragments_;
        size_t fragments_;
        size_t commit_fragments_;
        size_t rollback_fragments_;
    };
}


#endif // WSREP_MOCK_PROVIDER_HPP
