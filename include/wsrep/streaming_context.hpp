//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_STREAMING_CONTEXT_HPP
#define WSREP_STREAMING_CONTEXT_HPP

#include "logger.hpp"
#include "seqno.hpp"
#include "transaction_id.hpp"

#include <vector>

namespace wsrep
{
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
            , bytes_certified_()
            , unit_counter_()
        { }

        void enable(enum fragment_unit fragment_unit, size_t fragment_size)
        {
            wsrep::log_info() << "Enabling streaming: "
                              << fragment_unit << " " << fragment_size;
            assert(fragment_size > 0);
            fragment_unit_ = fragment_unit;
            fragment_size_ = fragment_size;
        }

        enum fragment_unit fragment_unit() const { return fragment_unit_; }

        size_t fragment_size() const { return fragment_size_; }

        void disable()
        {
            wsrep::log_info() << "Disabling streaming";
            fragment_size_ = 0;
        }

        void certified(size_t bytes)
        {
            ++fragments_certified_;
            bytes_certified_ += bytes;
        }

        size_t fragments_certified() const
        {
            return fragments_certified_;
        }

        void stored(wsrep::seqno seqno)
        {
            assert(seqno.is_undefined() == false);
            fragments_.push_back(seqno);
        }

        size_t fragments_stored() const
        {
            return fragments_.size();
        }

        void applied(wsrep::seqno seqno)
        {
            assert(seqno.is_undefined() == false);
            ++fragments_certified_;
            fragments_.push_back(seqno);
        }

        size_t bytes_certified() const
        {
            return bytes_certified_;
        }

        void rolled_back(wsrep::transaction_id id)
        {
            assert(rollback_replicated_for_ == wsrep::transaction_id::undefined());
            rollback_replicated_for_ = id;
        }

        bool rolled_back() const
        {
            return (rollback_replicated_for_ !=
                    wsrep::transaction_id::undefined());
        }

        size_t unit_counter() const
        {
            return unit_counter_;
        }

        void increment_unit_counter(size_t inc)
        {
            unit_counter_ += inc;
        }

        void reset_unit_counter()
        {
            unit_counter_ = 0;
        }

        const std::vector<wsrep::seqno>& fragments() const
        {
            return fragments_;
        }

        void cleanup()
        {
            fragments_certified_ = 0;
            fragments_.clear();
            rollback_replicated_for_ = wsrep::transaction_id::undefined();
            bytes_certified_ = 0;
            unit_counter_ = 0;
        }
    private:
        size_t fragments_certified_;
        std::vector<wsrep::seqno> fragments_;
        wsrep::transaction_id rollback_replicated_for_;
        enum fragment_unit fragment_unit_;
        size_t fragment_size_;
        size_t bytes_certified_;
        size_t unit_counter_;
    };
}

#endif // WSREP_STREAMING_CONTEXT_HPP
