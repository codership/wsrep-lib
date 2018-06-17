//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_WSREP_PROVIDER_V26_HPP
#define WSREP_WSREP_PROVIDER_V26_HPP

#include "wsrep/provider.hpp"

#include <wsrep_api.h>

namespace wsrep
{
    class wsrep_provider_v26 : public wsrep::provider
    {
    public:

        wsrep_provider_v26(wsrep::server_state&, const std::string&,
                           const std::string&);
        ~wsrep_provider_v26();
        int connect(const std::string&, const std::string&, const std::string&,
                    bool);
        int disconnect();

        enum wsrep::provider::status run_applier(void*);
        int start_transaction(wsrep::ws_handle&) { return 0; }
        int append_key(wsrep::ws_handle&, const wsrep::key&);
        int append_data(wsrep::ws_handle&, const wsrep::const_buffer&);
        enum wsrep::provider::status
        certify(wsrep::client_id, wsrep::ws_handle&,
                int,
                wsrep::ws_meta&);
        enum wsrep::provider::status
        bf_abort(wsrep::seqno,
                 wsrep::transaction_id,
                 wsrep::seqno&);
        int rollback(const wsrep::transaction_id) { ::abort(); return 0; }
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle&,
                           const wsrep::ws_meta&);
        int commit_order_leave(const wsrep::ws_handle&,
                               const wsrep::ws_meta&);
        int release(wsrep::ws_handle&);
        enum wsrep::provider::status replay(wsrep::ws_handle&, void*);
        int sst_sent(const wsrep::gtid&,int);
        int sst_received(const wsrep::gtid& gtid, int);

        std::vector<status_variable> status() const;
    private:
        wsrep_provider_v26(const wsrep_provider_v26&);
        wsrep_provider_v26& operator=(const wsrep_provider_v26);
        wsrep_t* wsrep_;
    };
}


#endif // WSREP_WSREP_PROVIDER_V26_HPP
