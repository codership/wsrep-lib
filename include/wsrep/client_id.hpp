//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_CLIENT_ID_HPP
#define WSREP_CLIENT_ID_HPP

#include <ostream>

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
        explicit client_id(I id)
            : id_(static_cast<type>(id))
        { }
        type get() const { return id_; }
        static type undefined() { return -1; }
        bool operator<(const client_id& other) const
        {
            return (id_ < other.id_);
        }
    private:
        type id_;
    };
    static inline std::ostream& operator<<(
        std::ostream& os, const wsrep::client_id& client_id)
    {
        return (os << client_id.get());
    }
}


#endif // WSREP_CLIENT_ID_HPP
