//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_MOCK_HIGH_PRIORITY_SERVICE_HPP
#define WSREP_MOCK_HIGH_PRIORITY_SERVICE_HPP

#include "wsrep/high_priority_service.hpp"
#include "mock_client_state.hpp"

namespace wsrep
{
    class mock_high_priority_service : public wsrep::high_priority_service
    {
    public:
        mock_high_priority_service(
            wsrep::server_state& server_state,
            wsrep::mock_client_state* client_state,
            bool replaying)
            : wsrep::high_priority_service(server_state)
            , fail_next_applying_()
            , fail_next_toi_()
            , client_state_(client_state)
            , replaying_(replaying)
        { }

        int apply(const wsrep::ws_handle&,
                  const wsrep::ws_meta&,
                  const wsrep::const_buffer&) WSREP_OVERRIDE;

        int start_transaction(const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;

        void adopt_transaction(const wsrep::transaction&) WSREP_OVERRIDE;
        int apply_write_set(const wsrep::ws_meta&,
                            const wsrep::const_buffer&) WSREP_OVERRIDE;
        int append_fragment_and_commit(
            const wsrep::ws_handle&,
            const wsrep::ws_meta&,
            const wsrep::const_buffer&) WSREP_OVERRIDE
        { return 0; }
        int remove_fragments(const wsrep::ws_meta&) WSREP_OVERRIDE
        { return 0; }
        int commit(const wsrep::ws_handle&, const wsrep::ws_meta&)
            WSREP_OVERRIDE;
        int rollback() WSREP_OVERRIDE;
        int apply_toi(const wsrep::ws_meta&,
                      const wsrep::const_buffer&) WSREP_OVERRIDE;
        void after_apply() WSREP_OVERRIDE;
        void store_globals() WSREP_OVERRIDE { }
        void reset_globals() WSREP_OVERRIDE { }
        void switch_execution_context(wsrep::high_priority_service&)
            WSREP_OVERRIDE { }
        int log_dummy_write_set(const wsrep::ws_handle&,
                                const wsrep::ws_meta&)
            WSREP_OVERRIDE { return 0; }
        bool is_replaying() const WSREP_OVERRIDE { return replaying_; }
        wsrep::mock_client_state* client_state()
        {
            return client_state_;
        }
        bool fail_next_applying_;
        bool fail_next_toi_;
    private:
        mock_high_priority_service(const mock_high_priority_service&);
        mock_high_priority_service& operator=(const mock_high_priority_service&);
        wsrep::mock_client_state* client_state_;
        bool replaying_;
    };
}

#endif // WSREP_MOCK_HIGH_PRIORITY_SERVICE_HPP
