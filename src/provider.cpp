//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/provider.hpp"

#include "provider_impl.hpp"

#include <wsrep_api.h>
#include <cerrno>

#include <sstream>

wsrep::id::id(const std::string& str)
    : type_(none)
    , data_()
{
    wsrep_uuid_t wsrep_uuid;
    if (wsrep_uuid_scan(str.c_str(), str.size(), &wsrep_uuid) ==
        WSREP_UUID_STR_LEN)
    {
        type_ = uuid;
        std::memcpy(data_, wsrep_uuid.data, sizeof(data_));
    }
    else if (str.size() <= 16)
    {
        type_ = string;
        std::memcpy(data_, str.c_str(), str.size());
    }
    std::ostringstream os;
    os << "String '" << str << "' does not contain UUID";
    throw wsrep::runtime_error(os.str());
}

wsrep::provider* wsrep::provider::make_provider(
    const std::string&)
{
    return 0;
}
