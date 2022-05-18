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

/** @file tls_provider_context.hpp
 *
 * An interface for controlling provider SSL/TLS settings.
 */

#ifndef WSREP_PROVIDER_TLS_CONTEXT_HPP
#define WSREP_PROVIDER_TLS_CONTEXT_HPP

#include <string>

#include "provider_options.hpp"

namespace wsrep
{
    class provider_tls_context
    {
    public:

        /**
         * TLS specific configuration parameters.
         */
        struct conf
        {
            /** If true, peer verification is turned on. */
            optional<bool> verify;
            /**
             * A file containing CA certificates. If empty, system
             * CA is used.
             */
            optional<std::string> ca_file;
            /**
             * File containing the certificate.
             */
            optional<std::string> cert_file;
            /**
             * File containing the private key.
             */
            optional<std::string> key_file;
            /**
             * A file containing password for private key.
             * Leave empty if private key is not password protected.
             */
            optional<std::string> password_file;

            conf()
                : verify{}
                , ca_file{}
                , cert_file{}
                , key_file{}
                , password_file{}
            {
            }

            conf(bool vfy, const std::string& caf, const std::string& certf,
                 const std::string& keyf, const std::string& passwdf)
                : verify{ vfy }
                , ca_file{ caf }
                , cert_file{ certf }
                , key_file{ keyf }
                , password_file{ passwdf }
            {
            }
        };

        /**
         * Construct a provider_tls_context.
         *
         * @param Reference to provider.
         */
        provider_tls_context() = default;

        /**
         * Destruct provider_tls_context.
         */
        virtual ~provider_tls_context() { }

        /**
         * Non-copy constructible and non-copy assignable.
         */
        provider_tls_context(const provider_tls_context&) = delete;
        provider_tls_context& operator=(const provider_tls_context&) = delete;

        /**
         * Check if the provider supports TLS.
         *
         * @return true If provider supports TLS.
         * @return false If provider does not support TLS.
         */
        virtual bool supports_tls() const = 0;

        /**
         * Check if TLS is enabled by the provider.
         *
         * @return true If TLS is enabled.
         * @return false If TLS is not enabled.
         */
        virtual bool is_enabled() const = 0;

        /**
         * Enable TLS on provider side.
         *
         * @return Zero in case of success
         * @return Non-zero in case of failure
         */
        virtual int enable() = 0;

        /**
         * Reload TLS context on provider side.
         *
         * @return Zero in case of success.
         * @return Non-zero in case of failure.
         */
        virtual int reload() = 0;

        /**
         * Set TLS specific configuration in provider.
         *
         * @return Zero in case of success.
         * @return Non-zero in case of failure.
         */
        virtual int configure(const conf&) = 0;

        /**
         * Get TLS specific configuration from provider.
         */
        virtual conf get_configuration() = 0;
    };
}

#endif // WSREP_PROVIDER_TLS_CONTEXT_HPP
