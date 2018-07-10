//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/key.hpp"
#include <ostream>
#include <iomanip>

namespace
{
    void print_key_part(std::ostream& os, const void* ptr, size_t len)
    {
        std::ios::fmtflags flags_save(os.flags());
        os << len << ": ";
        for (size_t i(0); i < len; ++i)
        {
            os << std::hex
               << std::setfill('0')
               << std::setw(2)
               << static_cast<int>(
                   *(reinterpret_cast<const unsigned char*>(ptr) + i)) << " ";
        }
        os.flags(flags_save);
    }
}

std::ostream& wsrep::operator<<(std::ostream& os, const wsrep::key& key)
{
    os << "type: " << key.type();
    for (size_t i(0); i < key.size(); ++i)
    {
        os << "\n    ";
        print_key_part(os, key.key_parts()[i].data(), key.key_parts()[i].size());
    }
    return os;
}
