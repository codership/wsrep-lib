//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef TRREP_PROVIDER_HPP
#define TRREP_PROVIDER_HPP

// #include "provider_impl.hpp"

#include <wsrep_api.h>

#include <string>

namespace trrep
{
    // Abstract interface for provider implementations
    class provider
    {
    public:
        virtual int start_transaction(wsrep_ws_handle_t*) = 0;
        virtual int append_key(wsrep_ws_handle_t*, const wsrep_key_t*) = 0;
        virtual int append_data(wsrep_ws_handle_t*, const wsrep_buf_t*) = 0;
        virtual wsrep_status_t
        certify(wsrep_conn_id_t, wsrep_ws_handle_t*,
                uint32_t,
                wsrep_trx_meta_t*) = 0;
        //!
        //! BF abort a transaction inside provider.
        //!
        //! @param[in] bf_seqno Seqno of the aborter transaction
        //! @param[in] victim_txt Transaction identifier of the victim
        //! @param[out] victim_seqno Sequence number of the victim transaction
        //!              or WSREP_SEQNO_UNDEFINED if the victim was not ordered
        //!
        //! @return wsrep_status_t
        virtual wsrep_status_t bf_abort(wsrep_seqno_t bf_seqno,
                                        wsrep_trx_id_t victim_trx,
                                        wsrep_seqno_t* victim_seqno) = 0;
        virtual int rollback(const wsrep_trx_id_t) = 0;
        virtual wsrep_status commit_order_enter(wsrep_ws_handle_t*) = 0;
        virtual int commit_order_leave(wsrep_ws_handle_t*) = 0;
        virtual int release(wsrep_ws_handle_t*) = 0;
        static provider* make_provider(const std::string& provider);

    };
}

#endif // TRREP_PROVIDER_HPP
