//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_STORAGE_SERVICE_HPP
#define WSREP_MOCK_STORAGE_SERVICE_HPP

#include "wsrep/storage_service.hpp"
#include "mock_client_state.hpp"

namespace wsrep
{
class mock_server_state;
    class mock_storage_service : public wsrep::storage_service
    {
    public:
        mock_storage_service(wsrep::mock_server_state&,
                                 wsrep::client_id);
        ~mock_storage_service();

        int start_transaction(const wsrep::ws_handle&) WSREP_OVERRIDE;

        void adopt_transaction(const wsrep::transaction&) WSREP_OVERRIDE;

        int append_fragment(const wsrep::id&,
                            wsrep::transaction_id,
                            int,
                            const wsrep::const_buffer&)
            WSREP_OVERRIDE
        { return 0; }

        int update_fragment_meta(const wsrep::ws_meta&) WSREP_OVERRIDE
        { return 0; }
        int remove_fragments() WSREP_OVERRIDE { return 0; }
        int commit(const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;

        int rollback(const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;

        void store_globals() WSREP_OVERRIDE { }
        void reset_globals() WSREP_OVERRIDE { }
    private:
        wsrep::mock_server_state& server_state_;
        wsrep::mock_client_service client_service_;
        wsrep::mock_client_state client_state_;
    };
}

#endif // WSREP_MOCK_STORAGE_SERVICE_HPP
