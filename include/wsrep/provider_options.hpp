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

/** @file provider_options.hpp
 *
 * A proxy interface to control provider configuration options.
 */

#ifndef WSREP_PROVIDER_OPTIONS_HPP
#define WSREP_PROVIDER_OPTIONS_HPP

#include "optional.hpp"
#include "provider.hpp"

#include <functional>
#include <map>
#include <string>
#include <memory>

namespace wsrep
{
    /**
     * Proxy class for managing provider options. All options values
     * are taken as strings, it is up to the user to know the type of
     * the value.
     */
    class provider_options
    {
    public:
        class option
        {
        public:
            option();
            /** Construct option with given values. Allocates
             * memory. */
            option(const std::string& name,
                   const std::string& value,
                   const std::string& default_value);
            /** Non copy-constructible. */
            option(const option&) = delete;
            /** Non copy-assignable. */
            option& operator=(const option&) = delete;
            option(option&&) = delete;
            option& operator=(option&&) = delete;
            ~option();

            /**
             * Get name of the option.
             *
             * @return Name of the option.
             */
            const char* name() const { return name_.c_str(); }

            /**
             * Get the real name of the option. This displays the
             * option name as it is visible in provider options.
             */
            const char* real_name() const { return real_name_.c_str(); }

            /**
             * Get value of the option.
             *
             * @return Value of the option.
             */
            const char* value() const { return value_.c_str(); }

            /**
             * Get default value of the option.
             *
             * @return Default value of the option.
             */
            const char* default_value() const { return default_value_.c_str(); }

            /**
             * Update the value of the option with new_value. The old
             * value is freed.
             */
            void update_value(const std::string& new_value);
        private:
            /** Sanitized name with dots replaced with underscores */
            std::string name_;
            /** Real name in provider */
            std::string real_name_;
            std::string  value_;
            std::string default_value_;
        };

        provider_options(wsrep::provider&);
        provider_options(const provider_options&) = delete;
        provider_options& operator=(const provider_options&) = delete;

        /**
         * Set initial options. This should be used to initialize
         * provider options after the provider has been loaded.
         * Individual options should be accessed through set()/get().
         *
         * @return Provider status code.
         */
        enum wsrep::provider::status initial_options();

        /**
         * Update options from string.
         */
        // enum wsrep::provider::status options(const std::string&);

        /**
         * Get the value of the option.
         *
         * @param name Option name
         */
        wsrep::optional<const char*> get(const std::string& name) const;

        /**
         * Set a value for the option.
         *
         * @return Wsrep provider status code.
         * @return wsrep::provider::error_size_exceeded if memory could
         *         not be allocated for the new value.
         */
        enum wsrep::provider::status
        set(const std::string& name, const std::string& value);

        /**
         * Create a new option with default value.
         */
        enum wsrep::provider::status
        set_default(const std::string& name, const std::string& value);

        void for_each(const std::function<void(option*)>& fn);
    private:
        provider& provider_;
        using options_map = std::map<std::string, std::unique_ptr<option>>;
        options_map options_;
    };

    /**
     * Equality operator for provider option. Returns true if
     * the name part is equal.
     */
    bool operator==(const wsrep::provider_options::option&,
                    const wsrep::provider_options::option&);
}

#endif // WSREP_PROVIDER_OPTIONS_HPP
