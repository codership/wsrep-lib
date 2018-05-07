//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "trrep/provider.hpp"

#include "provider_impl.hpp"

trrep::provider* trrep::provider::make_provider(
     const std::string&)
{
    return 0;
}
