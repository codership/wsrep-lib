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

#include "wsrep/transaction.hpp"
#include "mock_client_state.hpp"
#include "mock_high_priority_service.hpp"

int wsrep::mock_client_service::bf_rollback()
{
    int ret(0);
    if (client_state_->before_rollback())
    {
        ret = 1;
    }
    else if (client_state_->after_rollback())
    {
        ret = 1;
    }
    return ret;
}

struct replayer_context
{
    wsrep::mock_client_state state;
    wsrep::mock_client_service service;
    replayer_context(wsrep::server_state& server_state,
                     const wsrep::transaction& transaction,
                     const wsrep::client_id& id)
        : state{server_state, service, id, wsrep::client_state::m_high_priority}
        , service{&state}
    {
        state.open(id);
        state.before_command();
        state.clone_transaction_for_replay(transaction);
    }

    ~replayer_context() {
        state.after_applying();
        state.after_command_before_result();
        state.after_command_after_result();
        state.close();
    }
};

enum wsrep::provider::status
wsrep::mock_client_service::replay()
{
    /* Mimic application and allocate separate client state for replaying. */
    wsrep::client_id replayer_id{ 1001 };
    replayer_context replayer(client_state_->server_state(),
                              client_state_->transaction(), replayer_id);
    wsrep::mock_high_priority_service hps{ client_state_->server_state(),
                                           &replayer.state, true };

    enum wsrep::provider::status ret(
        client_state_->provider().replay(
            replayer.state.transaction().ws_handle(),
            &hps));
    ++replays_;
    return ret;
}
