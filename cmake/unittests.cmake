# Copyright (c) 2024, Codership Oy.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License, version 2.0,
# as published by the Free Software Foundation.
#
# This program is also distributed with certain software (including
# but not limited to OpenSSL) that is licensed under separate terms,
# as designated in a particular file or component or in included license
# documentation.  The authors of MySQL hereby grant you an additional
# permission to link the program and your derivative works with the
# separately licensed software that they have included with MySQL.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License, version 2.0, for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

if (NOT WSREP_LIB_WITH_UNIT_TESTS)
  return()
endif()

set(MIN_BOOST_VERSION "1.54.0")
if (WSREP_LIB_WITH_UNIT_TESTS_EXTRA)
  message(STATUS "Compiling extra unit tests")
  set(json_HEADER "boost/json/src.hpp")
  # Extra tests may require packages from very recent boost which may be
  # unavailable on the system. In such case download private boost distro.
  check_include_file_cxx(${json_HEADER} system_json_FOUND)
  if (NOT system_json_FOUND)
    if (NOT WITH_BOOST)
      set(WITH_BOOST "${CMAKE_SOURCE_DIR}/third_party/boost")
    endif()
    set(DOWNLOAD_BOOST ON)
    include (cmake/boost.cmake)
    set(MIN_BOOST_VERSION "${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_PATCH}")
    message(STATUS "Boost includes: ${BOOST_INCLUDE_DIR}, ver: ${MIN_BOOST_VERSION}")
    find_package(Boost ${MIN_BOOST_VERSION} REQUIRED
      COMPONENTS json headers unit_test_framework
      PATHS ${WITH_BOOST}/lib/cmake
      NO_DEFAULT_PATH
      )
    # Include as system header to be more permissive about generated
    # warnings.
    set(CMAKE_REQUIRED_FLAGS "-isystem ${BOOST_INCLUDE_DIR}")
    check_include_file_cxx(${json_HEADER} json_FOUND)
    if (NOT json_FOUND)
      message(FATAL_ERROR "Required header 'boost/json.hpp' not found: ${json_FOUND}")
    else()
      include_directories(SYSTEM ${BOOST_INCLUDE_DIR})
    endif()
  endif()
  return()
endif() # WSREP_LIB_WITH_UNIT_TESTS_EXTRA

# Superproject has already detected Boost. Most likely MySQL 8.0 or later.
if (BOOST_INCLUDE_DIR)
  message(STATUS "Boost include dir set to ${BOOST_INCLUDE_DIR}")
  cmake_push_check_state()
  list(APPEND CMAKE_REQUIRED_INCLUDES "${BOOST_INCLUDE_DIR}")
  # Append -isystem to tolerate warnings from compiler
  list(APPEND CMAKE_REQUIRED_FLAGS "-isystem ${BOOST_INCLUDE_DIR}")
  check_cxx_source_compiles(
    "
#define BOOST_TEST_ALTERNATIVE_INIT_API
#include <boost/test/included/unit_test.hpp>
bool init_unit_test() { return true; }
"
    FOUND_BOOST_TEST_INCLUDED_UNIT_TEST_HPP)
  if (NOT FOUND_BOOST_TEST_INCLUDED_UNIT_TEST_HPP)
    message(FATAL_ERROR
      "Boost unit test header file not found from ${CMAKE_REQUIRED_INCLUDES}")
  endif()
  cmake_pop_check_state()
  include_directories(SYSTEM ${BOOST_INCLUDE_DIR})
  return()
endif() # BOOST_INCLUDE_DIR

# System Boost.
find_package(Boost ${MIN_BOOST_VERSION}
  REQUIRED
  COMPONENTS unit_test_framework
  )
