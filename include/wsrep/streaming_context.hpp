//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_STREAMING_CONTEXT_HPP
#define WSREP_STREAMING_CONTEXT_HPP

#include "logger.hpp"

namespace wsrep
{
    class streaming_context
    {
    public:
        enum fragment_unit
        {
            row,
            bytes,
            statement
        };

        streaming_context()
            : fragments_()
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
            fragment_size_ = 0;
        }

        void certified(wsrep::seqno seqno)
        {
            fragments_.push_back(seqno);
        }

        size_t fragments_certified() const
        {
            return fragments_.size();
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

        size_t unit_counter() const { return unit_counter_; }
        void increment_unit_counter(size_t inc)
        { unit_counter_ += inc; }
        void reset_unit_counter() { unit_counter_ = 0; }
        const std::vector<wsrep::seqno>& fragments() const
        {
            return fragments_;
        }
        void cleanup()
        {
            fragments_.clear();
            rollback_replicated_for_ = wsrep::transaction_id::undefined();
            bytes_certified_ = 0;
            unit_counter_ = 0;
        }
    private:
        std::vector<wsrep::seqno> fragments_;
        wsrep::transaction_id rollback_replicated_for_;
        enum fragment_unit fragment_unit_;
        size_t fragment_size_;
        size_t bytes_certified_;
        size_t unit_counter_;
    };
}

#endif // WSREP_STREAMING_CONTEXT_HPP
