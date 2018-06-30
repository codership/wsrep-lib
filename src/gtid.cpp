//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/gtid.hpp"

#include <cerrno>
#include <iostream>
#include <sstream>

std::ostream& wsrep::operator<<(std::ostream& os, const wsrep::gtid& gtid)
{
    return (os << gtid.id() << ":" << gtid.seqno());
}

std::istream& wsrep::operator>>(std::istream& is, wsrep::gtid& gtid)
{
    std::string id_str;
    std::getline(is, id_str, ':');
    long long seq;
    is >> seq;
    gtid = wsrep::gtid(wsrep::id(id_str), wsrep::seqno(seq));
    std::cout << "GTID: " << gtid << "\n";
    return is;
}

ssize_t wsrep::gtid_print_to_c_str(
    const wsrep::gtid& gtid, char* buf, size_t buf_len)
{
    std::ostringstream os;
    os << gtid;
    if (os.str().size() > buf_len)
    {
        return -ENOBUFS;
    }
    std::strncpy(buf, os.str().c_str(), os.str().size());
    return os.str().size();
}
