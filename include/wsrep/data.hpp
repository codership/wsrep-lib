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
        }
        data(const void* ptr, size_t len)
            : buf_(ptr, len)
        {
        }
        const wsrep::buffer& get() const { return buf_; }
    private:
        wsrep::buffer buf_;
    };
}

#endif // WSREP_DATA_HPP
