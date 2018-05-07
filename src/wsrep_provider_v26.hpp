//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_WSREP_PROVIDER_V26_HPP
#define TRREP_WSREP_PROVIDER_V26_HPP

#include "trrep/provider.hpp"

#include <wsrep_api.h>

namespace trrep
{
    class wsrep_provider_v26 : public trrep::provider
    {
    public:

        wsrep_provider_v26(const char*, struct wsrep_init_args*);
        ~wsrep_provider_v26();
        int connect(const std::string&, const std::string&, const std::string&,
                    bool);
        int disconnect();

        wsrep_status_t run_applier(void*);
        int start_transaction(wsrep_ws_handle_t*) { return 0; }
        int append_key(wsrep_ws_handle_t*, const wsrep_key_t*);
        int append_data(wsrep_ws_handle_t*, const wsrep_buf_t*);
        wsrep_status_t
        certify(wsrep_conn_id_t, wsrep_ws_handle_t*,
                uint32_t,
                wsrep_trx_meta_t*);
        wsrep_status_t bf_abort(wsrep_seqno_t,
                                wsrep_trx_id_t,
                                wsrep_seqno_t*);
        int rollback(const wsrep_trx_id_t) { ::abort(); return 0; }
        wsrep_status commit_order_enter(const wsrep_ws_handle_t*,
                                        const wsrep_trx_meta_t*);
        int commit_order_leave(const wsrep_ws_handle_t*,
                               const wsrep_trx_meta_t*);
        int release(wsrep_ws_handle_t*);
        int replay(wsrep_ws_handle_t*, void*);
        int sst_sent(const wsrep_gtid_t&,int);
        int sst_received(const wsrep_gtid_t& gtid, int);

        std::vector<status_variable> status() const;
    private:
        wsrep_provider_v26(const wsrep_provider_v26&);
        wsrep_provider_v26& operator=(const wsrep_provider_v26);
        struct wsrep* wsrep_;
    };
}


#endif // TRREP_WSREP_PROVIDER_V26_HPP
