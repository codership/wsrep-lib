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

#include "../src/galera_tls_context.hpp"
#include "mock_provider.hpp"
#include "mock_server_state.hpp"

#include <boost/test/unit_test.hpp>

//
// Options are initialize from options string which have Galera
// native format, ie. component.variable. As this string
// is passed to initial_options(), it is translated to form
// with dots replaced by underscores. Therefore all the verification
// steps should see variables with underscores only format.
//

struct galera_tls_context_fixture
{
private:
    wsrep::mock_server_service server_service;
    wsrep::mock_server_state server_state;
    wsrep::mock_provider provider;
public:
    wsrep::provider_options options;
    std::unique_ptr<wsrep::galera_tls_context> tls_context;
    galera_tls_context_fixture()
        : server_service( server_state )
        , server_state( "test", wsrep::server_state::rm_async, server_service )
        , provider( server_state )
        , options(provider)
        , tls_context()
    {
        options.initial_options("socket.ssl=YES");
        tls_context = std::unique_ptr<wsrep::galera_tls_context>(
            new wsrep::galera_tls_context{ options });
    }
};

struct galera_tls_context_not_enabled_fixture :
    public galera_tls_context_fixture
{
    galera_tls_context_not_enabled_fixture()
        : galera_tls_context_fixture()
    {
        options.initial_options("socket.ssl=NO");
        tls_context->get_configuration();
        BOOST_REQUIRE(not tls_context->is_enabled());
    }
};

struct galera_tls_context_reload_fixture :
    public galera_tls_context_fixture
{
    galera_tls_context_reload_fixture()
        : galera_tls_context_fixture()
    {
        options.initial_options("socket.ssl=YES;socket.ssl_reload=NO;");
        tls_context->get_configuration();
        BOOST_REQUIRE(tls_context->is_enabled());
    }
};

struct galera_tls_context_full_config_fixture :
    public galera_tls_context_fixture
{
    galera_tls_context_full_config_fixture()
        : galera_tls_context_fixture()
    {
        options.initial_options("socket.ssl=YES;"
                                "socket.ssl_verify=YES;"
                                "socket.ssl_ca=ca_file;"
                                "socket.ssl_cert=cert_file;"
                                "socket.ssl_key=key_file;"
                                "socket.ssl_password=password_file;");
        tls_context->get_configuration();
        BOOST_REQUIRE(tls_context->is_enabled());
    }
};

struct galera_tls_context_no_ssl_fixture :
    public galera_tls_context_fixture
{
    galera_tls_context_no_ssl_fixture()
        : galera_tls_context_fixture{ }
    {
        options.initial_options("");
        tls_context->get_configuration();
        BOOST_REQUIRE(not tls_context->is_enabled());
    }
};


BOOST_FIXTURE_TEST_CASE(galera_tls_context, galera_tls_context_fixture) {}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_supports_tls,
                        galera_tls_context_fixture)
{
    BOOST_REQUIRE(tls_context->supports_tls());
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_is_enabled,
                        galera_tls_context_fixture)
{
    BOOST_REQUIRE(tls_context->is_enabled());
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_no_verify,
                        galera_tls_context_fixture)
{
    auto conf ( tls_context->get_configuration() );
    BOOST_REQUIRE_MESSAGE(options.options() == "socket_ssl=YES;",
                          options.options() << " != socket_ssl=YES;");
    // If socket.verify is not supported by the provider, the
    // peer verification is automatically on.
    BOOST_REQUIRE(conf.verify);
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_disable_verify,
                        galera_tls_context_fixture)
{
    options.initial_options("socket.ssl=YES;socket.ssl_verify=YES;");
    (void)tls_context->get_configuration();
    wsrep::tls_context::conf conf{ false, "", "", "", "" };
    BOOST_REQUIRE(tls_context->configure(conf) == 0);
    BOOST_REQUIRE_MESSAGE(options.options()
                              == "socket_ssl=YES;socket_ssl_verify=NO;",
                          options.options() << " != socket_ssl=YES;socket_ssl_verify=NO;");
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_unsupported_option,
                        galera_tls_context_fixture)
{
    auto conf ( tls_context->get_configuration() );
    // Default fixture does not have key_file set.
    BOOST_REQUIRE(not conf.key_file);
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_enable,
                        galera_tls_context_not_enabled_fixture)
{
    BOOST_REQUIRE(tls_context->enable() == 0);
    BOOST_REQUIRE_MESSAGE(options.options() == "socket_ssl=YES;",
                          options.options() << " != socket_ssl=YES;");
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_reload,
                        galera_tls_context_reload_fixture)
{
    tls_context->reload();
    BOOST_REQUIRE_MESSAGE(options.options() == "socket_ssl=YES;socket_ssl_reload=YES;",
                          options.options() << " != socket_ssl=YES;socket_ssl_reload=YES;");
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_full_config,
                        galera_tls_context_full_config_fixture)
{
    auto conf ( tls_context->get_configuration() );
    BOOST_REQUIRE(tls_context->is_enabled());
    BOOST_REQUIRE(conf.verify);
    BOOST_REQUIRE(conf.ca_file);
    BOOST_REQUIRE(conf.cert_file);
    BOOST_REQUIRE(conf.key_file);
    BOOST_REQUIRE(conf.password_file);
    auto provider_options_before ( options.options() );
    tls_context->configure(conf);
    BOOST_REQUIRE_MESSAGE(provider_options_before == options.options(),
                          provider_options_before << " != " << options.options());
}

BOOST_FIXTURE_TEST_CASE(galera_tls_context_no_ssl,
                        galera_tls_context_no_ssl_fixture)
{
    auto conf ( tls_context->get_configuration() );
    BOOST_REQUIRE(not tls_context->is_enabled());
    BOOST_REQUIRE(not conf.verify);
    BOOST_REQUIRE(not conf.ca_file);
    BOOST_REQUIRE(not conf.cert_file);
    BOOST_REQUIRE(not conf.key_file);
    BOOST_REQUIRE(not conf.password_file);
}
