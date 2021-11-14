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

#include "event_service_v1.hpp"

#include "wsrep/event_service.hpp"
#include "wsrep/reporter.hpp"
#include "wsrep/logger.hpp"
#include "v26/wsrep_event_service.h"
#include "service_helpers.hpp"

#include <cassert>

namespace wsrep_event_service_v1
{
    static std::atomic_flag initialized = ATOMIC_FLAG_INIT;

    static void callback(
        wsrep_event_context_t* ctx,
        const char* name,
        const char* value)
    {
        if (ctx)
        {
            wsrep::event_service* const impl
                (reinterpret_cast<wsrep::event_service*>(ctx));
            impl->process_event(name, value);
        }
    }

    static const char* const log_string = "event service v1";
}

int wsrep::event_service_v1_probe(void* dlh)
{
    typedef int (*init_fn)(wsrep_event_service_v1_t*);
    typedef void (*deinit_fn)();
    if (wsrep_impl::service_probe<init_fn>(
            dlh, WSREP_EVENT_SERVICE_INIT_FUNC_V1,
            wsrep_event_service_v1::log_string) ||
        wsrep_impl::service_probe<deinit_fn>(
            dlh, WSREP_EVENT_SERVICE_DEINIT_FUNC_V1,
            wsrep_event_service_v1::log_string))
    {
        // diagnostic message was logged by wsrep_impl::service_probe()
        return 1;
    }
    return 0;
}

int wsrep::event_service_v1_init(void* dlh,
                                 wsrep::event_service* event_service)
{
    if (not (dlh && event_service)) return EINVAL;

    if (wsrep_event_service_v1::initialized.test_and_set()) return EALREADY;

    wsrep_event_service_v1_t service =
    {
        wsrep_event_service_v1::callback,
        reinterpret_cast<wsrep_event_context_t*>(event_service)
    };

    typedef int (*init_fn)(wsrep_event_service_v1_t*);
    int const ret(wsrep_impl::service_init<init_fn>(
                      dlh, WSREP_EVENT_SERVICE_INIT_FUNC_V1, &service,
                      wsrep_event_service_v1::log_string));
    if (ret)
    {
        wsrep_event_service_v1::initialized.clear();
    }

    return ret;
}

void wsrep::event_service_v1_deinit(void* dlh)
{
    if (wsrep_event_service_v1::initialized.test_and_set())
    {
        // service was initialized
        typedef int (*deinit_fn)();
        wsrep_impl::service_deinit<deinit_fn>(
            dlh, WSREP_EVENT_SERVICE_DEINIT_FUNC_V1,
            wsrep_event_service_v1::log_string);
    }

    wsrep_event_service_v1::initialized.clear();
}
