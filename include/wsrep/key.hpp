//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_KEY_HPP
#define WSREP_KEY_HPP

#include "exception.hpp"

namespace wsrep
{
    class key
    {
    public:
        key()
            : key_parts_()
            , key_()
        {
            key_.key_parts = key_parts_;
            key_.key_parts_num = 0;
        }

        void append_key_part(const void* ptr, size_t len)
        {
            if (key_.key_parts_num == 3)
            {
                throw wsrep::runtime_error("key parts exceed maximum of 3");
            }
            key_parts_[key_.key_parts_num].ptr = ptr;
            key_parts_[key_.key_parts_num].len = len;
            ++key_.key_parts_num;
        }

        const wsrep_key_t& get() const { return key_; }
    private:
        wsrep_buf_t key_parts_[3];
        wsrep_key_t key_;
    };
}

#endif // WSREP_KEY_HPP
