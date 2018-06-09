//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_BUFFER_HPP
#define WSREP_BUFFER_HPP

namespace wsrep
{
    class buffer
    {
    public:
        buffer()
            : ptr_()
            , size_()
        { }
        buffer(const void* ptr, size_t size)
            : ptr_(ptr)
            , size_(size)
        { }

        const void* ptr() const { return ptr_; }
        size_t size() const { return size_; }

    private:
        const void* ptr_;
        size_t size_;
    };
}

#endif // WSREP_BUFFER_HPP
