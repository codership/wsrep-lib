//
// Copyright (C) 2018 Codership Oy <info@codership.com>
//

#include "wsrep/provider.hpp"
#include "wsrep/logger.hpp"

#include "mock_provider.hpp"
#include "wsrep_provider_v26.hpp"

wsrep::provider* wsrep::provider::make_provider(
    wsrep::server_context& server_context,
    const std::string& provider_spec,
    const std::string& provider_options)
{
    try
    {
        if (provider_spec == "mock")
        {
            return  new wsrep::mock_provider(server_context);
        }
        else
        {
            return new wsrep::wsrep_provider_v26(
                server_context, provider_options, provider_spec);
        }
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
