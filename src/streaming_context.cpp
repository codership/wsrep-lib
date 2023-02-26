/*
 * Copyright (C) 2023 Codership Oy <info@codership.com>
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

#include "wsrep/streaming_context.hpp"

#include <cassert>

void wsrep::streaming_context::params(enum fragment_unit fragment_unit,
                                      size_t fragment_size)
{
    if (fragment_size)
    {
        WSREP_LOG_DEBUG(
            wsrep::log::debug_log_level(), wsrep::log::debug_level_streaming,
            "Enabling streaming: " << fragment_unit << " " << fragment_size);
    }
    else
    {
        WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                        wsrep::log::debug_level_streaming,
                        "Disabling streaming");
    }
    fragment_unit_ = fragment_unit;
    fragment_size_ = fragment_size;
    reset_unit_counter();
}

void wsrep::streaming_context::enable(enum fragment_unit fragment_unit,
                                      size_t fragment_size)
{
    WSREP_LOG_DEBUG(
        wsrep::log::debug_log_level(), wsrep::log::debug_level_streaming,
        "Enabling streaming: " << fragment_unit << " " << fragment_size);
    assert(fragment_size > 0);
    fragment_unit_ = fragment_unit;
    fragment_size_ = fragment_size;
}

void wsrep::streaming_context::disable()
{
    WSREP_LOG_DEBUG(wsrep::log::debug_log_level(),
                    wsrep::log::debug_level_streaming, "Disabling streaming");
    fragment_size_ = 0;
}

void wsrep::streaming_context::stored(wsrep::seqno seqno)
{
    check_fragment_seqno(seqno);
    fragments_.push_back(seqno);
}

void wsrep::streaming_context::applied(wsrep::seqno seqno)
{
    check_fragment_seqno(seqno);
    ++fragments_certified_;
    fragments_.push_back(seqno);
}

void wsrep::streaming_context::rolled_back(wsrep::transaction_id id)
{
    assert(rollback_replicated_for_ == wsrep::transaction_id::undefined());
    rollback_replicated_for_ = id;
}

void wsrep::streaming_context::cleanup()
{
    fragments_certified_ = 0;
    fragments_.clear();
    rollback_replicated_for_ = wsrep::transaction_id::undefined();
    unit_counter_ = 0;
    log_position_ = 0;
}

void wsrep::streaming_context::check_fragment_seqno(
    wsrep::seqno seqno WSREP_UNUSED)
{
    assert(seqno.is_undefined() == false);
    assert(fragments_.empty() || fragments_.back() < seqno);
}
