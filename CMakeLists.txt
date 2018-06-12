#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

cmake_minimum_required (VERSION 2.8)
project (wsrep-lib)
include(CheckIncludeFile)
include(CTest)

# Options
option(WITH_AUTO_TEST "Run unit tests automatically after build" ON)
option(WITH_ASAN "Enable address sanitizer" OFF)
option(WITH_TSAN "Enable thread sanitizer" OFF)

# CXX flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Weffc++ -Woverloaded-virtual -Wno-non-virtual-dtor -g")

check_include_file("${CMAKE_CURRENT_SOURCE_DIR}/wsrep/wsrep_api.h" HAVE_WSREP_API_HPP)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/wsrep-API/v26")
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
if (WITH_ASAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()
if (WITH_TSAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

add_custom_target(coverage_report
  lcov --capture --directory src --output lcov.info --no-external
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
add_subdirectory(wsrep-API)
add_subdirectory(test)
add_subdirectory(dbsim)

