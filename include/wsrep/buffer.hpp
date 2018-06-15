//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_BUFFER_HPP
#define WSREP_BUFFER_HPP

#include <cstddef>
#include <vector>

namespace wsrep
{
    class const_buffer
    {
    public:
        const_buffer()
            : ptr_()
            , size_()
        { }

        const_buffer(const void* ptr, size_t size)
            : ptr_(ptr)
            , size_(size)
        { }

        const void* ptr() const { return ptr_; }
        const void* data() const { return ptr_; }
        size_t size() const { return size_; }

    private:
        // const_buffer(const const_buffer&);
        // const_buffer& operator=(const const_buffer&);
        const void* ptr_;
        size_t size_;
    };


    class mutable_buffer
    {
    public:
        mutable_buffer()
            : buffer_()
        { }

        void push_back(const char* begin, const char* end)
        {
            buffer_.insert(buffer_.end(), begin, end);
        }
        const char* data() const { return &buffer_[0]; }
        size_t size() const { return buffer_.size(); }
    private:
        std::vector<char> buffer_;
    };

}

#endif // WSREP_BUFFER_HPP
