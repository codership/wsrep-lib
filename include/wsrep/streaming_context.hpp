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

#ifndef WSREP_STREAMING_CONTEXT_HPP
#define WSREP_STREAMING_CONTEXT_HPP

#include "compiler.hpp"
#include "logger.hpp"
#include "seqno.hpp"
#include "transaction_id.hpp"

#include <vector>

namespace wsrep
{
    /* Helper class to store streaming transaction context. */
    class streaming_context
    {
    public:
        enum fragment_unit
        {
            bytes,
            row,
            statement
        };

        streaming_context()
            : fragments_certified_()
            , fragments_()
            , rollback_replicated_for_()
            , fragment_unit_()
            , fragment_size_()
            , unit_counter_()
            , log_position_()
        { }

        /**
         * Set streaming parameters.
         *
         * Calling this method has a side effect of resetting unit
         * counter.
         *
         * @param fragment_unit Desired fragment unit.
         * @param fragment_size Desired fragment size.
         */
        void params(enum fragment_unit fragment_unit, size_t fragment_size);

        /**
         * Enable streaming replication.
         *
         * @param fragment_unit Desired fragment unit.
         * @param fragment_size Desired fragment size.
         */
        void enable(enum fragment_unit fragment_unit, size_t fragment_size);

        /** Return current fragment unit. */
        enum fragment_unit fragment_unit() const { return fragment_unit_; }

        /** Return current fragment size. */
        size_t fragment_size() const { return fragment_size_; }

        /** Disable streaming replication. */
        void disable();

        /** Increment counter for certified fragments. */
        void certified()
        {
            ++fragments_certified_;
        }

        /** Return number of certified fragments. */
        size_t fragments_certified() const
        {
            return fragments_certified_;
        }

        /** Mark fragment with seqno as stored in fragment store. */
        void stored(wsrep::seqno seqno);

        /** Return number of stored fragments. */
        size_t fragments_stored() const
        {
            return fragments_.size();
        }

        /** Mark fragment with seqno as applied. */
        void applied(wsrep::seqno seqno);

        /** Mark streaming transaction as rolled back. */
        void rolled_back(wsrep::transaction_id id);

        /** Return true if streaming transaction has been marked
         * as rolled back. */
        bool rolled_back() const
        {
            return (rollback_replicated_for_ !=
                    wsrep::transaction_id::undefined());
        }

        /** Return current value of unit counter. */
        size_t unit_counter() const
        {
            return unit_counter_;
        }

        /** Set value for unit counter. */
        void set_unit_counter(size_t count)
        {
            unit_counter_ = count;
        }

        /** Increment unit counter by inc. */
        void increment_unit_counter(size_t inc)
        {
            unit_counter_ += inc;
        }

        /** Reset unit counter to zero. */
        void reset_unit_counter()
        {
            unit_counter_ = 0;
        }

        /** Return current log position. */
        size_t log_position() const
        {
            return log_position_;
        }

        /** Set log position. */
        void set_log_position(size_t position)
        {
            log_position_ = position;
        }

        /** Return vector of stored fragments. */
        const std::vector<wsrep::seqno>& fragments() const
        {
            return fragments_;
        }

        /** Return true if the fragment size was exceeded. */
        bool fragment_size_exceeded() const
        {
            return unit_counter_ >= fragment_size_;
        }

        /** Clean up the streaming transaction state. */
        void cleanup();
    private:

        void check_fragment_seqno(wsrep::seqno seqno);

        size_t fragments_certified_;
        std::vector<wsrep::seqno> fragments_;
        wsrep::transaction_id rollback_replicated_for_;
        enum fragment_unit fragment_unit_;
        size_t fragment_size_;
        size_t unit_counter_;
        size_t log_position_;
    };
}

#endif // WSREP_STREAMING_CONTEXT_HPP
