//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/provider.hpp"
#include "wsrep/logger.hpp"

#include "wsrep_provider_v26.hpp"

wsrep::provider* wsrep::provider::make_provider(
    wsrep::server_state& server_state,
    const std::string& provider_spec,
    const std::string& provider_options)
{
    try
    {
        return new wsrep::wsrep_provider_v26(
            server_state, provider_options, provider_spec);
    }
    catch (const wsrep::runtime_error& e)
    {
        wsrep::log_error() << "Failed to create a new provider '"
                           << provider_spec << "'"
                           << " with options '" << provider_options
                           << "':" << e.what();
    }
    catch (...)
    {
        wsrep::log_error() << "Caught unknown exception when trying to "
                           << "create a new provider '"
                           << provider_spec << "'"
                           << " with options '" << provider_options;
    }
    return 0;
}
