//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_storage_service.hpp"
#include "mock_server_state.hpp"

#include "wsrep/client_state.hpp"

wsrep::mock_storage_service::mock_storage_service(
    wsrep::mock_server_state& server_state,
    wsrep::client_id client_id)
    : server_state_(server_state)
    , client_service_(client_state_)
    , client_state_(server_state, client_service_, client_id,
                    wsrep::client_state::m_high_priority)
{
    client_state_.open(client_id);
    client_state_.before_command();
}


wsrep::mock_storage_service::~mock_storage_service()
{
    client_state_.after_command_before_result();
    client_state_.after_command_after_result();
    client_state_.close();
    client_state_.cleanup();
}

int wsrep::mock_storage_service::start_transaction()
{
    return client_state_.start_transaction(
        server_state_.next_transaction_id());
}

int wsrep::mock_storage_service::commit(const wsrep::ws_handle& ws_handle,
                                        const wsrep::ws_meta& ws_meta)
{
    return client_state_.prepare_for_fragment_ordering(
        ws_handle, ws_meta, true) ||
        client_state_.before_commit() ||
        client_state_.ordered_commit() ||
        client_state_.after_commit() ||
        client_state_.after_statement();
}

int wsrep::mock_storage_service::rollback(const wsrep::ws_handle& ws_handle,
                                          const wsrep::ws_meta& ws_meta)
{
    return client_state_.prepare_for_fragment_ordering(
        ws_handle, ws_meta, false) ||
        client_state_.before_rollback() ||
        client_state_.after_rollback() ||
        client_state_.after_statement();
}
