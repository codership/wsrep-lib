//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/seqno.hpp"
#include <ostream>

std::ostream& wsrep::operator<<(std::ostream& os, wsrep::seqno seqno)
{
    return (os << seqno.get());
}
