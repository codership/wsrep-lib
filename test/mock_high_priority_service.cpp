//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "mock_high_priority_service.hpp"
#include "mock_server_state.hpp"

int wsrep::mock_high_priority_service::apply(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta,
    const wsrep::const_buffer& data) WSREP_OVERRIDE
{
    return server_state_.on_apply(*this, ws_handle, ws_meta, data);
}

int wsrep::mock_high_priority_service::start_transaction(
    const wsrep::ws_handle& ws_handle, const wsrep::ws_meta& ws_meta)
{
    return client_state_->start_transaction(ws_handle, ws_meta);
}

void wsrep::mock_high_priority_service::adopt_transaction(
    const wsrep::transaction& transaction)
{
    client_state_->adopt_transaction(transaction);
}

int wsrep::mock_high_priority_service::apply_write_set(
    const wsrep::ws_meta&,
    const wsrep::const_buffer&)
{
    assert(client_state_->toi_meta().seqno().is_undefined());
    assert(client_state_->transaction().state() == wsrep::transaction::s_executing ||
           client_state_->transaction().state() == wsrep::transaction::s_replaying);
    return (fail_next_applying_ ? 1 : 0);
}

int wsrep::mock_high_priority_service::commit(const wsrep::ws_handle&,
                                              const wsrep::ws_meta&)
{

    int ret(0);
    if (client_state_->client_service().do_2pc())
    {
        ret = client_state_->before_prepare() ||
            client_state_->after_prepare();
    }
    return (ret || client_state_->before_commit() ||
            client_state_->ordered_commit() ||
            client_state_->after_commit());
}

int wsrep::mock_high_priority_service::rollback()
{
    return (client_state_->before_rollback() ||
            client_state_->after_rollback());
}

int wsrep::mock_high_priority_service::apply_toi(const wsrep::ws_meta&,
                                                 const wsrep::const_buffer&)
{
    assert(client_state_->transaction().active() == false);
    assert(client_state_->toi_meta().seqno().is_undefined() == false);
    return (fail_next_toi_ ? 1 : 0);
}

void wsrep::mock_high_priority_service::after_apply()
{
    client_state_->after_applying();
}
