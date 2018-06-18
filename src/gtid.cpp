//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/gtid.hpp"

#include <iostream>

std::ostream& wsrep::operator<<(std::ostream& os, const wsrep::gtid& gtid)
{
    return (os << gtid.id() << ":" << gtid.seqno());
}
