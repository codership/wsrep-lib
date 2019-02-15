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

#include "mock_high_priority_service.hpp"
#include "mock_server_state.hpp"

wsrep::client_state& wsrep::mock_high_priority_service::client_state()
{
    return *client_state_;
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

int wsrep::mock_high_priority_service::commit(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    int ret(0);
    client_state_->prepare_for_ordering(ws_handle, ws_meta, true);
    if (do_2pc_)
    {
        ret = client_state_->before_prepare() ||
            client_state_->after_prepare();
    }
    return (ret || client_state_->before_commit() ||
            client_state_->ordered_commit() ||
            client_state_->after_commit());
}

int wsrep::mock_high_priority_service::rollback(
    const wsrep::ws_handle& ws_handle,
    const wsrep::ws_meta& ws_meta)
{
    client_state_->prepare_for_ordering(ws_handle, ws_meta, false);
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
