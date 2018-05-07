#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

cmake_minimum_required (VERSION 2.8)
project (trrep)
include(CheckIncludeFile)
include(CTest)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Weffc++ -Woverloaded-virtual -Wno-non-virtual-dtor -g")

check_include_file("${CMAKE_CURRENT_SOURCE_DIR}/wsrep/wsrep_api.h" HAVE_WSREP_API_HPP)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/wsrep")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/wsrep")


find_package(Boost 1.54.0 REQUIRED
  unit_test_framework
  program_options
  filesystem
  thread
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


if (WITH_DOCUMENTATION)
  find_package(Doxygen REQUIRED)
  add_custom_target(doc ALL
    COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc
    COMMENT "Generating documentation with Doxygen"
    VERBATIM)
endif()

add_subdirectory(src)
add_subdirectory(wsrep)

