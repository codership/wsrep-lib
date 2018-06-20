//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_KEY_HPP
#define WSREP_KEY_HPP

#include "exception.hpp"
#include "buffer.hpp"

namespace wsrep
{
    class key
    {
    public:
        enum type
        {
            shared,
            semi_shared,
            semi_exclusive,
            exclusive
        };

        key(enum type type)
            : type_(type)
            , key_parts_()
            , key_parts_len_()
        { }

        void append_key_part(const void* ptr, size_t len)
        {
            if (key_parts_len_ == 3)
            {
                throw wsrep::runtime_error("key parts exceed maximum of 3");
            }
            key_parts_[key_parts_len_] = wsrep::const_buffer(ptr, len);
            ++key_parts_len_;
        }

        enum type type() const
        {
            return type_;
        }

        size_t size() const
        {
            return key_parts_len_;
        }

        const wsrep::const_buffer* key_parts() const
        {
            return key_parts_;
        }
    private:

        enum type type_;
        wsrep::const_buffer key_parts_[3];
        size_t key_parts_len_;
    };

    typedef std::vector<wsrep::key> key_array;

}

#endif // WSREP_KEY_HPP
