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

std::string wsrep::flags_to_string(int flags)
{
    std::ostringstream oss;
    if (flags & provider::flag::start_transaction)
        oss << "start_transaction | ";
    if (flags & provider::flag::commit)
        oss << "commit | ";
    if (flags & provider::flag::rollback)
        oss << "rollback | ";
    if (flags & provider::flag::isolation)
        oss << "isolation | ";
    if (flags & provider::flag::pa_unsafe)
        oss << "pa_unsafe | ";
    if (flags & provider::flag::snapshot)
        oss << "snapshot | ";

    std::string ret(oss.str());
    if (ret.size() > 3) ret.erase(ret.size() - 3);
    return ret;
}
