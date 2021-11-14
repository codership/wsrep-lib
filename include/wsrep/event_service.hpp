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


/** @file event_service.hpp
 *
 * Service interface for providing events to DBMS.
 */

#ifndef WSREP_EVENT_SERVICE_HPP
#define WSREP_EVENT_SERVICE_HPP

#include <string>

namespace wsrep
{

    /** @class event_service
     *
     * Event service interface. This provides an interface corresponding
     * to wsrep-API event service. For details see
     * wsrep-API/wsrep_event_service.h
     */
    class event_service
    {
    public:
        virtual ~event_service() { }

        /**
         * Process event with name name and value value.
         */
        virtual void process_event(const std::string& name,
                                   const std::string& value) = 0;
    };
}

#endif // WSREP_EVENT_SERVICE_HPP
