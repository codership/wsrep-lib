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
#include "wsrep/logger.hpp"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <vector>

static std::vector<std::string> tokenize(const std::string& real_str,
                                         char const sep, char const esc)
{
    // The string is modified in place when parsing, make a copy
    std::string str{ real_str };
    std::vector<std::string> ret;
    std::string sep_esc;
    sep_esc += sep;
    sep_esc += esc;
    size_t prev_pos{ 0 };
    size_t pos{ str.find_first_of(sep_esc, prev_pos) };

    if (real_str.empty())
        return ret;

    // No separator found, return with one element containing input string
    if (pos == std::string::npos)
    {
        ret.push_back(str);
        return ret;
    }

    do
    {
        assert(pos >= prev_pos);
        if (str[pos] == esc)
        {
            // String ends in escape character, break loop
            if (pos + 1 == str.size())
            {
                ++pos;
                break;
            };

            if (str[pos + 1] == sep) // Skip escape
            {
                str.erase(pos, 1);
                ++pos;
                if (pos >= str.size())
                    break;
            }
        }
        else if (str[pos]
                 == sep) // Separator found, push [prev_pos, pos) to ret
        {
            ret.push_back(str.substr(prev_pos, pos - prev_pos));
            ++pos;
            prev_pos = pos;
        }
    } while ((pos = str.find_first_of(sep_esc, pos)) != std::string::npos);
    // No sparator found from the tail of the string, push remaining from
    // last scanned position until the end
    if (prev_pos < str.size())
    {
        ret.push_back(str.substr(prev_pos, str.size() - prev_pos));
    }
    return ret;
}

static std::string& strip(std::string& str)
{
    str.erase(std::remove_if(str.begin(), str.end(), ::isspace), str.end());
    return str;
}

// Replace dots in option name with underscores
static void sanitize_name(std::string& name)
{
    std::transform(name.begin(), name.end(), name.begin(),
                   [](std::string::value_type c)
                       {
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
    : name_{},
      real_name_{},
      value_{},
      default_value_{}
{
}

wsrep::provider_options::option::option(const std::string& name,
                                        const std::string& value,
                                        const std::string& default_value)
    : name_{ name }
    , real_name_{ name }
    , value_{ value }
    , default_value_{ default_value }
{
    sanitize_name(name_);
}

void wsrep::provider_options::option::update_value(const std::string& value)
{
    value_ = value;
}

wsrep::provider_options::option::~option()
{
}

wsrep::provider_options::provider_options(wsrep::provider& provider)
    : provider_( provider )
    , options_()
{
}

enum wsrep::provider::status
wsrep::provider_options::initial_options(const std::string& opts_str)
{
    options_.clear();
    auto ret( provider_.options(opts_str) );
    if (ret == wsrep::provider::success)
    {

        auto real_opts_str( provider_.options() );
        parse_provider_options(real_opts_str, [this](const std::string& key,
                                                     const std::string& value) {
            // assert(not key.empty());
            wsrep::log_debug() << "key: " << key << " value: " << value;
            if (key.empty())
                return;
            auto opt( std::unique_ptr<option>(
                          new option{ key, value, value }) );
            options_.emplace(std::string(opt->name()), std::move(opt));
        });
    }
    return ret;
}

std::string wsrep::provider_options::options() const
{
    std::string ret;
    const provider_options_sep sep;
    std::for_each(options_.cbegin(), options_.cend(),
                  [&ret, &sep](const options_map::value_type& opt) {
                      ret += std::string(opt.second->name()) + sep.key_value
                             + std::string(opt.second->value()) + sep.param;
                  });

    return ret;
}

enum wsrep::provider::status
wsrep::provider_options::set(const std::string& name, const std::string& value)
{
    auto option( options_.find(name) );
    if (option == options_.end())
    {
        return not_found_error;
    }
    provider_options_sep sep;
    wsrep::log_debug() << "Setting option wit real name: "
                       << option->second->real_name();
    auto ret( provider_.options(std::string(option->second->real_name())
                                + sep.key_value + value + sep.param) );
    if (ret == provider::success)
    {
        option->second->update_value(value);
    }
    return ret;
}

wsrep::optional<const char*>
wsrep::provider_options::get(const std::string& name) const
{
    auto option( options_.find(name) );
    if (option == options_.end())
    {
        return optional<const char*>{};
    }
    return optional<const char*>(option->second->value());
}

int wsrep::parse_provider_options(
    const std::string& options_str,
    const std::function<void(const std::string&, const std::string&)>& on_value)
try
{
    provider_options_sep sep;
    auto params( tokenize(options_str, sep.param, sep.escape) );
    std::for_each(params.cbegin(), params.cend(),
                  [&on_value, &sep](const std::string& param) {
                      auto keyval( tokenize(param, sep.key_value, sep.escape) );
                      // All Galera options are form of <key> = <value>
                      if (keyval.size() > 2)
                          return;
                      const auto& key( strip(keyval[0]) );
                      if (keyval.size() == 2)
                      {
                          const auto& value( strip(keyval[1]) );
                          on_value(key, value);
                      }
                      else
                      {
                          // TODO: Currently this causes parameters without '='
                          // separator being parsed as parameters with empty
                          // value.
                          on_value(key, "");
                      }
                  });
    return 0;
}
catch (const std::exception&)
{
    return 1;
}

void wsrep::provider_options::for_each(const std::function<void(option*)>& fn)
{
    std::for_each(
        options_.begin(), options_.end(),
        [&fn](const options_map::value_type& opt) { fn(opt.second.get()); });
}
