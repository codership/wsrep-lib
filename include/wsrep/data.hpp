//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_DATA_HPP
#define WSREP_DATA_HPP

namespace wsrep
{
    class data
    {
    public:
        data()
            : buf_()
        {
            assign(0, 0);
        }
        data(const void* ptr, size_t len)
            : buf_()
        {
            assign(ptr, len);
        }

        void assign(const void* ptr, size_t len)
        {
            buf_.ptr = ptr;
            buf_.len = len;
        }

        const wsrep_buf_t& get() const { return buf_; }
    private:
        wsrep_buf_t buf_;
    };
}

#endif // WSREP_DATA_HPP
