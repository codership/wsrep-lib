//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

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
        int connect(const std::string&, const std::string&, const std::string&,
                    bool);
        int disconnect();
        int capabilities() const;

        int desync();
        int resync();
        int pause();
        int resume();

        enum wsrep::provider::status run_applier(void*);
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
        int rollback(const wsrep::transaction_id) { ::abort(); return 0; }
        enum wsrep::provider::status
        commit_order_enter(const wsrep::ws_handle&,
                           const wsrep::ws_meta&);
        int commit_order_leave(const wsrep::ws_handle&,
                               const wsrep::ws_meta&);
        int release(wsrep::ws_handle&);
        enum wsrep::provider::status replay(const wsrep::ws_handle&, void*);
        int sst_sent(const wsrep::gtid&,int);
        int sst_received(const wsrep::gtid& gtid, int);

        std::vector<status_variable> status() const;
        void reset_status();
        std::string options() const;
        void options(const std::string&);
        void* native() const;
    private:
        wsrep_provider_v26(const wsrep_provider_v26&);
        wsrep_provider_v26& operator=(const wsrep_provider_v26);
        struct wsrep_st* wsrep_;
    };
}


#endif // WSREP_WSREP_PROVIDER_V26_HPP
