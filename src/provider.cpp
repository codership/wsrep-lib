//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/provider.hpp"

#include "provider_impl.hpp"

wsrep::provider* wsrep::provider::make_provider(
     const std::string&)
{
    return 0;
}
