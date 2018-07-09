//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#ifndef WSREP_SR_KEY_SET_HPP
#define WSREP_SR_KEY_SET_HPP

#include <set>
#include <map>

namespace wsrep
{
    class sr_key_set
    {
    public:
        typedef std::set<std::string> leaf_type;
        typedef std::map<std::string, leaf_type > branch_type;
        sr_key_set()
            : root_()
        { }

        void insert(const wsrep::key& key)
        {
            assert(key.size() == 3);
            if (key.size() < 3)
            {
                throw wsrep::runtime_error("Invalid key size");
            }

            root_[std::string(
                    static_cast<const char*>(key.key_parts()[0].data()),
                    key.key_parts()[0].size())].insert(
                        std::string(static_cast<const char*>(key.key_parts()[1].data()),
                                    key.key_parts()[1].size()));
        }

        const branch_type& root() const { return root_; }
        void clear() { root_.clear(); }
    private:
        branch_type root_;
    };
}

#endif // WSREP_KEY_SET_HPP
