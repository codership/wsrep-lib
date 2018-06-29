//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/gtid.hpp"

#include <iostream>

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
