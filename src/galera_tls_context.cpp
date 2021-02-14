/*
 * Copyright (C) 2021 Codership Oy <info@codership.com>
 *
 * This file is part of wsrep-lib.
 *
 * Wsrep-lib is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * Wsrep-lib is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wsrep-lib.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "galera_tls_context.hpp"

#include "wsrep/logger.hpp"
#include "wsrep/provider.hpp"
#include "wsrep/provider_options.hpp"

#include <algorithm>
#include <cctype>
#include <unordered_set>

namespace
{

    static std::string galera_bool_false{ "NO" };
    static std::string galera_bool_true{ "YES" };

    // Option names visible here are mangled ones with dots replaced
    // by underscores.

    // Known SSL Config options
    const std::string galera_socket_ssl{ "socket_ssl" };
    const std::string galera_socket_ssl_verify{ "socket_ssl_verify" };
    const std::string galera_socket_ssl_ca{ "socket_ssl_ca" };
    const std::string galera_socket_ssl_cert{ "socket_ssl_cert" };
    const std::string galera_socket_ssl_key{ "socket_ssl_key" };
    const std::string galera_socket_ssl_password{ "socket_ssl_password" };

    // SSL reload option
    const std::string galera_socket_ssl_reload{ "socket_ssl_reload" };

    std::unordered_set<std::string> galera_ssl_params{
        galera_socket_ssl,     galera_socket_ssl_verify,
        galera_socket_ssl_ca,  galera_socket_ssl_cert,
        galera_socket_ssl_key, galera_socket_ssl_password
    };

    // True values extracted from galerautils/src/gu_utils.c
    static std::unordered_set<std::string> galera_bool_true_vals
    {
        "1", "y",
        "on",
        "yes", "yep",
        "true", "sure", "yeah"
    };

    static bool read_boolean(const std::string& str)
    {
        auto lower(str);
        std::transform(
            lower.begin(), lower.end(), lower.begin(),
            [](std::string::value_type c) { return std::tolower(c); });
        return (galera_bool_true_vals.find(lower)
                != galera_bool_true_vals.end());
    }

    static bool known_ssl_key(const std::string& key)
    {
        return (galera_ssl_params.find(key) != galera_ssl_params.end());
    }

    template <class Type>
    void
    assign_value(const wsrep::galera_tls_context::tls_options_map_type& options,
                 const std::string& key, wsrep::optional<Type>& value)
    {
        auto option( options.find(key) );
        if (option != options.end() && not option->second.empty())
        {
            std::istringstream iss{ option->second };
            Type val{};
            iss >> val;
            value = wsrep::optional<Type>(val);
        }
    }

    template <>
    void
    assign_value(const wsrep::galera_tls_context::tls_options_map_type& options,
                 const std::string& key, wsrep::optional<bool>& value)
    {
        auto option( options.find(key) );
        if (option != options.end() && not option->second.empty())
        {
            bool val{ read_boolean(option->second) };
            value = wsrep::optional<bool>(val);
        }
    }
} // namespace

wsrep::galera_tls_context::galera_tls_context(
    wsrep::provider_options& provider_options)
    : provider_options_(provider_options)
    , options_()
{
    galera_tls_context::get_configuration();
}

wsrep::galera_tls_context::~galera_tls_context() {}

bool wsrep::galera_tls_context::supports_tls() const
{
    return (options_.find(galera_socket_ssl) != options_.end());
}

bool wsrep::galera_tls_context::is_enabled() const
{
    bool ret{ };
    if (supports_tls())
    {
        ret = read_boolean(options_.find(galera_socket_ssl)->second);
    }
    return ret;
}

int wsrep::galera_tls_context::enable()
{
    if (provider_options_.set(galera_socket_ssl, galera_bool_true)
        != wsrep::provider::success)
    {
        return 1;
    }
    // Re-read configuration from provider
    get_configuration();
    return 0;
}

int wsrep::galera_tls_context::reload()
{
    return (provider_options_.set(galera_socket_ssl_reload, galera_bool_true)
            != wsrep::provider::success);
}

int wsrep::galera_tls_context::configure(
    const wsrep::galera_tls_context::conf& conf)
{
    int ret{ 0 };
    if (has_option(galera_socket_ssl))
        ret |= provider_options_.set(
            galera_socket_ssl,
            (is_enabled() ? galera_bool_true : galera_bool_false));
    if (has_option(galera_socket_ssl_verify) && conf.verify)
        ret |= provider_options_.set(
            galera_socket_ssl_verify,
            (conf.verify.value() ? galera_bool_true : galera_bool_false));
    if (has_option(galera_socket_ssl_ca) && conf.ca_file)
        ret |= provider_options_.set(galera_socket_ssl_ca,
                                     conf.ca_file.value());
    if (has_option(galera_socket_ssl_cert) && conf.cert_file)
        ret |= provider_options_.set(galera_socket_ssl_cert,
                                     conf.cert_file.value());
    if (has_option(galera_socket_ssl_key) && conf.key_file)
        ret |= provider_options_.set(galera_socket_ssl_key,
                                     conf.key_file.value());
    if (has_option(galera_socket_ssl_password) && conf.password_file)
        ret |= provider_options_.set(galera_socket_ssl_password,
                                     conf.password_file.value());

    return (ret ? wsrep::provider::error_warning : 0);
}

wsrep::galera_tls_context::conf
wsrep::galera_tls_context::get_configuration()
{
    auto options_str( provider_options_.options() );
    if (options_str.empty())
    {
        options_.clear();
        return conf{};
    }

    tls_options_map_type options;
    if (wsrep::parse_provider_options(
            options_str,
            [&options](const std::string& key, const std::string& value) {
                if (known_ssl_key(key))
                    options.insert(std::make_pair(key, value));
            }))
    {
        wsrep::log_warning() << "Parsing configuration string failed";
        return conf{};
    }

    options_ = options;
    if (wsrep::log::debug_log_level())
    {
        std::ostringstream oss;
        oss << "Found TLS options from provider:\n";
        std::for_each(options_.begin(), options_.end(),
                      [&oss](const tls_options_map_type::value_type& option) {
                          oss << "  " << option.first << " = " << option.second
                              << "\n";
                      });
    }

    conf ret;
    // Galera versions prior to 4.8 don't have verify option and verify
    // is enabled by default.
    if (not has_option(galera_socket_ssl_verify))
        ret.verify = optional<bool>(true);
    else
    {
        assign_value<bool>(options_, galera_socket_ssl_verify, ret.verify);
    }
    if (has_option(galera_socket_ssl_ca))
        assign_value<std::string>(options_, galera_socket_ssl_ca, ret.ca_file);
    if (has_option(galera_socket_ssl_cert))
        assign_value<std::string>(options_, galera_socket_ssl_cert,
                                  ret.cert_file);
    if (has_option(galera_socket_ssl_key))
        assign_value<std::string>(options_, galera_socket_ssl_key,
                                  ret.key_file);
    if (has_option(galera_socket_ssl_password))
        assign_value<std::string>(options_, galera_socket_ssl_password,
                                  ret.password_file);
    return ret;
}

bool wsrep::galera_tls_context::has_option(const std::string& key) const
{
    return (options_.find(key) != options_.end());
}
