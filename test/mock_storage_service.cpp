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

int wsrep::mock_storage_service::start_transaction(
    const wsrep::ws_handle& ws_handle)
{
    return client_state_.start_transaction(ws_handle.transaction_id());
}

void wsrep::mock_storage_service::adopt_transaction(
    const wsrep::transaction& transaction)
{
    client_state_.adopt_transaction(transaction);
}

int wsrep::mock_storage_service::commit(const wsrep::ws_handle& ws_handle,
                                        const wsrep::ws_meta& ws_meta)
{
    int ret(client_state_.prepare_for_ordering(
                ws_handle, ws_meta, true) ||
            client_state_.before_commit() ||
            client_state_.ordered_commit() ||
            client_state_.after_commit());
    client_state_.after_applying();
    return ret;
}

int wsrep::mock_storage_service::rollback(const wsrep::ws_handle& ws_handle,
                                          const wsrep::ws_meta& ws_meta)
{
    int ret(client_state_.prepare_for_ordering(
                ws_handle, ws_meta, false) ||
            client_state_.before_rollback() ||
            client_state_.after_rollback());
    client_state_.after_applying();
    return ret;
}
