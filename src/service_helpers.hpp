/*
 * Copyright (C) 2020 Codership Oy <info@codership.com>
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

#ifndef WSREP_SERVICE_HELPERS_HPP
#define WSREP_SERVICE_HELPERS_HPP

#include "wsrep/logger.hpp"

#include <dlfcn.h>
#include <cerrno>

namespace wsrep_impl
{
    template <typename InitFn>
    int service_probe(void* dlh, const char* symbol, const char* service_name)
    {
        union {
            InitFn dlfun;
            void* obj;
        } alias;
        // Clear previous errors
        (void)dlerror();
        alias.obj = dlsym(dlh, symbol);
        if (alias.obj)
        {
            return 0;
        }
        else
        {
            wsrep::log_warning() << "Support for " << service_name
                                 << " " << symbol
                                 << " not found from provider: "
                                 << dlerror();
            return ENOTSUP;
        }
    }


    template <typename InitFn, typename ServiceCallbacks>
    int service_init(void* dlh,
                     const char* symbol,
                     ServiceCallbacks service_callbacks,
                     const char* service_name)
    {
        union {
            InitFn dlfun;
            void* obj;
        } alias;
        // Clear previous errors
        (void)dlerror();
        alias.obj = dlsym(dlh, symbol);
        if (alias.obj)
        {
            wsrep::log_info() << "Initializing " << service_name;
            return (*alias.dlfun)(service_callbacks);
        }
        else
        {
            wsrep::log_info()
                << "Provider does not support " << service_name;
            return ENOTSUP;
        }
    }

    template <typename DeinitFn>
    void service_deinit(void* dlh, const char* symbol, const char* service_name)
    {
        union {
            DeinitFn dlfun;
            void* obj;
        } alias;
        // Clear previous errors
        (void)dlerror();
        alias.obj = dlsym(dlh, symbol);
        if (alias.obj)
        {
            wsrep::log_info() << "Deinitializing " << service_name;
            (*alias.dlfun)();
        }
        else
        {
            wsrep::log_info()
                << "Provider does not support deinitializing of "
                << service_name;
        }
    }
}

#endif // WSREP_SERVICE_HELPERS_HPP

