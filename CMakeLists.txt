#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

cmake_minimum_required (VERSION 2.8)
project (wsrep-lib)
include(CheckIncludeFile)
include(CheckCXXCompilerFlag)

# Options

# Compile unit tests
option(WSREP_LIB_WITH_UNIT_TESTS "Compile unit tests" ON)
if (WSREP_LIB_WITH_UNIT_TESTS)
  # Run tests automatically by default if compiled
  option(WSREP_LIB_WITH_AUTO_TEST "Run unit tests automatically after build" OFF)
endif()

# Build a sample program
option(WSREP_LIB_WITH_DBSIM "Compile sample dbsim program" ON)

option(WSREP_LIB_WITH_ASAN "Enable address sanitizer" OFF)
option(WSREP_LIB_WITH_TSAN "Enable thread sanitizer" OFF)

option(WSREP_LIB_WITH_DOCUMENTATION "Generate documentation" OFF)
option(WSREP_LIB_WITH_COVERAGE "Compile with coverage instrumentation" OFF)

option(WSREP_LIB_STRICT_BUILD_FLAGS "Compile with strict build flags")

# CXX flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Woverloaded-virtual -g")

CHECK_CXX_COMPILER_FLAG("-Wimplicit-fallthrough" COMPILER_SUPPORTS_IMPLICIT_FALLTHROUGH)
if (COMPILER_SUPPORTS_IMPLICIT_FALLTHROUGH)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wimplicit-fallthrough")
endif()

if (WSREP_LIB_STRICT_BUILD_FLAGS)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Weffc++")
endif()

check_include_file("${CMAKE_CURRENT_SOURCE_DIR}/wsrep/wsrep_api.h" HAVE_WSREP_API_HPP)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include")
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/wsrep-API/v26")
link_directories("${CMAKE_CURRENT_SOURCE_DIR}/wsrep")

if (WSREP_LIB_WITH_UNIT_TESTS)
  find_package(Boost 1.54.0 REQUIRED
    unit_test_framework
    )
endif()

if (WSREP_LIB_WITH_DBSIM)
  find_package(Boost 1.54.0 REQUIRED
    program_options
    filesystem
    thread
    )
endif()

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
if (WSREP_LIB_WITH_COVERAGE)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fprofile-arcs -ftest-coverage")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fprofile-arcs -ftest-coverage")
endif()
if (WSREP_LIB_WITH_ASAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address")
endif()
if (WSREP_LIB_WITH_TSAN)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=thread")
endif()

add_custom_target(coverage_report
  lcov --capture --directory src --directory test --directory dbsim --directory include --output lcov.info --no-external
  COMMAND genhtml --output-directory coverage_report lcov.info)


if (WSREP_LIB_WITH_DOCUMENTATION)
  find_package(Doxygen REQUIRED)
  add_custom_target(doc ALL
    COMMAND doxygen ${CMAKE_CURRENT_SOURCE_DIR}/doc/Doxyfile
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/doc
    COMMENT "Generating documentation with Doxygen"
    VERBATIM)
endif()

add_subdirectory(src)
add_subdirectory(wsrep-API)
if (WSREP_LIB_WITH_UNIT_TESTS)
  enable_testing()
  add_subdirectory(test)
endif()
if (WSREP_LIB_WITH_DBSIM)
  add_subdirectory(dbsim)
endif()
