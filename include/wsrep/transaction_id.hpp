//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_TRANSACTION_ID_HPP
#define WSREP_TRANSACTION_ID_HPP

#include <iostream>

namespace wsrep
{
    class transaction_id
    {
    public:
        typedef unsigned long long type;


        transaction_id()
            : id_(-1)
        { }

        template <typename I>
        explicit transaction_id(I id)
            : id_(static_cast<type>(id))
        { }
        type get() const { return id_; }
        static transaction_id undefined() { return transaction_id(-1); }
        bool is_undefined() const { return (id_ == type(-1)); }
        bool operator<(const transaction_id& other) const
        {
            return (id_ < other.id_);
        }
        bool operator==(const transaction_id& other) const
        { return (id_ == other.id_); }
        bool operator!=(const transaction_id& other) const
        { return (id_ != other.id_); }
    private:
        type id_;
    };

    static inline std::ostream& operator<<(std::ostream& os, transaction_id id)
    {
        return (os << id.get());
    }
}

#endif // WSREP_TRANSACTION_ID_HPP
