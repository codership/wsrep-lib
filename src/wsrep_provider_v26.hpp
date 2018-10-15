/*
 * Copyright (C) 2018 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef WSREP_WSREP_PROVIDER_V26_HPP
#define WSREP_WSREP_PROVIDER_V26_HPP

#include "wsrep/provider.hpp"

struct wsrep_st;

namespace wsrep
{
    class wsrep_provider_v26 : public wsrep::provider
    {
    public:

        wsrep_provider_v26(wsrep::server_state&, const std::string&,
                           const std::string&);
        ~wsrep_provider_v26();
        enum wsrep::provider::status
        connect(const std::string&, const std::string&, const std::string&,
                    bool);
        int disconnect();
        int capabilities() const;

        int desync();
        int resync();
        wsrep::seqno pause();
        int resume();

        enum wsrep::provider::status run_applier(wsrep::high_priority_service*);
        int start_transaction(wsrep::ws_handle&) { return 0; }
        int append_key(wsrep::ws_handle&, const wsrep::key&);
        enum wsrep::provider::status
        append_data(wsrep::ws_handle&, const wsrep::const_buffer&);
        enum wsrep::provider::status
        certify(wsrep::client_id, wsrep::ws_handle&,
                int,
                wsrep::ws_meta&);
        enum wsrep::provider::status
        bf_abort(wsrep::seqno,
                 wsrep::transaction_id,
                 wsrep::seqno&);
        enum wsrep::provider::status rollback(const wsrep::transaction_id);
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle&,
                           const wsrep::ws_meta&);
        int commit_order_leave(const wsrep::ws_handle&,
                               const wsrep::ws_meta&);
        int release(wsrep::ws_handle&);
        enum wsrep::provider::status replay(const wsrep::ws_handle&,
                                            wsrep::high_priority_service*);
        enum wsrep::provider::status enter_toi(wsrep::client_id,
                                               const wsrep::key_array&,
                                               const wsrep::const_buffer&,
                                               wsrep::ws_meta&,
                                               int);
        enum wsrep::provider::status leave_toi(wsrep::client_id);
        std::pair<wsrep::gtid, enum wsrep::provider::status>
        causal_read(int) const;
        enum wsrep::provider::status wait_for_gtid(const wsrep::gtid&, int) const;
        wsrep::gtid last_committed_gtid() const;
        int sst_sent(const wsrep::gtid&,int);
        int sst_received(const wsrep::gtid& gtid, int);

        std::vector<status_variable> status() const;
        void reset_status();
        std::string options() const;
        enum wsrep::provider::status options(const std::string&);
        void* native() const;
    private:
        wsrep_provider_v26(const wsrep_provider_v26&);
        wsrep_provider_v26& operator=(const wsrep_provider_v26);
        struct wsrep_st* wsrep_;
    };
}


#endif // WSREP_WSREP_PROVIDER_V26_HPP
