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

#include "wsrep/provider_options.hpp"
#include "config_service_v1.hpp"
#include "wsrep/logger.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>

/**
 * Provider options string separators.
 */
struct provider_options_sep
{
    /** Parameter separator. */
    char param{ ';' };
    /** Key value separator. */
    char key_value{ '=' };
};

// Replace dots in option name with underscores
static void sanitize_name(std::string& name)
{
    std::transform(name.begin(), name.end(), name.begin(),
                   [](std::string::value_type c) {
                       if (c == '.')
                           return '_';
                       return c;
                   });
}

bool wsrep::operator==(const wsrep::provider_options::option& left,
                       const wsrep::provider_options::option& right)
{
    return (std::strcmp(left.name(), right.name()) == 0);
}

// wsrep-API lacks better error code for not found, and this is
// what Galera returns when parameter is not recogized, so we
// got with it.
static enum wsrep::provider::status not_found_error{
    wsrep::provider::error_warning
};

wsrep::provider_options::option::option()
    : name_{}
    , real_name_{}
    , value_{}
    , default_value_{}
    , flags_{ 0 }
{
}

wsrep::provider_options::option::option(
    const std::string& name,
    std::unique_ptr<wsrep::provider_options::option_value> value,
    std::unique_ptr<wsrep::provider_options::option_value> default_value,
    int flags)
    : name_{ name }
    , real_name_{ name }
    , value_{ std::move(value) }
    , default_value_{ std::move(default_value) }
    , flags_{ flags }
{
    sanitize_name(name_);
}

void wsrep::provider_options::option::update_value(
    std::unique_ptr<wsrep::provider_options::option_value> value)
{
    value_ = std::move(value);
}

wsrep::provider_options::option::~option() {}

wsrep::provider_options::provider_options(wsrep::provider& provider)
    : provider_(provider)
    , options_()
{
}

enum wsrep::provider::status wsrep::provider_options::initial_options()
{
    options_.clear();
    if (config_service_v1_fetch(provider_, this))
    {
        return wsrep::provider::error_not_implemented;
    }
    else
    {
        return wsrep::provider::success;
    }
}

const wsrep::provider_options::option*
wsrep::provider_options::get_option(const std::string& name) const
{
    auto ret(options_.find(name));
    if (ret == options_.end())
    {
        return nullptr;
    }
    return ret->second.get();
}

enum wsrep::provider::status wsrep::provider_options::set(
    const std::string& name,
    std::unique_ptr<wsrep::provider_options::option_value> value)
{
    auto option(options_.find(name));
    if (option == options_.end())
    {
        return not_found_error;
    }
    provider_options_sep sep;
    auto ret(provider_.options(std::string(option->second->real_name())
                               + sep.key_value + value->as_string()
                               + sep.param));
    if (ret == provider::success)
    {
        option->second->update_value(std::move(value));
    }
    return ret;
}

enum wsrep::provider::status wsrep::provider_options::set_default(
    const std::string& name,
    std::unique_ptr<wsrep::provider_options::option_value> value,
    std::unique_ptr<wsrep::provider_options::option_value> default_value,
    int flags)
{
    auto found(options_.find(name));
    auto opt(std::unique_ptr<provider_options::option>(
        new option{ name, std::move(value), std::move(default_value), flags }));
    if (found != options_.end())
    {
        assert(0);
        return wsrep::provider::error_not_allowed;
    }
    options_.emplace(std::string(opt->name()), std::move(opt));
    return wsrep::provider::success;
}

void wsrep::provider_options::for_each(const std::function<void(option*)>& fn)
{
    std::for_each(
        options_.begin(), options_.end(),
        [&fn](const options_map::value_type& opt) { fn(opt.second.get()); });
}
