//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_CLIENT_ID_HPP
#define WSREP_CLIENT_ID_HPP

namespace wsrep
{
    class client_id
    {
    public:
        typedef unsigned long long type;
        client_id()
            : id_(-1)
        { }
        template <typename I>
        client_id(I id)
            : id_(static_cast<type>(id))
        { }
        type get() const { return id_; }
        static type undefined() { return -1; }
    private:
        type id_;
    };
}

#endif // WSREP_CLIENT_ID_HPP
