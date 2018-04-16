#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

cmake_minimum_required (VERSION 2.8)
project (wsrep-cpp)
include(CheckIncludeFile)
include(CTest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Weffc++ -Woverloaded-virtual -Wno-non-virtual-dtor -g")

if (NOT WITH_WSREP_API)
  message(ERROR "Wsrep API location must be provided")
else()
  check_include_file("${WITH_WSREP_API}/wsrep_api.h" HAVE_WSREP_API_HPP)
  include_directories("${WITH_WSREP_API}")
endif()

find_package(Boost 1.54.0 REQUIRED
  unit_test_framework
  )

# Coverage
# To produce a coverage report, call cmake with -DWITH_COVERAGE=ON,
# run
#
#   make
#   make test
#   make ExperimentalCoverage
#   make coverage_report
#
# The coverage report output will be in directory root index.html
#
if (WITH_COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()
add_custom_target(coverage_report
  lcov --capture --directory . --output lcov.info --no-external
  COMMAND genhtml --output-directory coverage_report lcov.info)


add_subdirectory(src)
