# Copyright (c) 2014, 2021, Oracle and/or its affiliates.
# Copyright (c) 2021, Codership OY.
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

# We need boost verision >= 1.75 to test JSON-related code.

# Invoke with -DWITH_BOOST=<directory> or set WITH_BOOST in environment.
# If WITH_BOOST is *not* set, or is set to the special value "system",
# we assume that the correct version (see below)
# is installed on the compile host in the standard location.

FUNCTION(GET_FILE_SIZE FILE_NAME OUTPUT_SIZE)
  IF(WIN32)
    FILE(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/cmake/filesize.bat" FILESIZE_BAT)
    FILE(TO_NATIVE_PATH "${FILE_NAME}" NATIVE_FILE_NAME)

    EXECUTE_PROCESS(
      COMMAND "${FILESIZE_BAT}" "${NATIVE_FILE_NAME}"
      RESULT_VARIABLE COMMAND_RESULT
      OUTPUT_VARIABLE RESULT
      OUTPUT_STRIP_TRAILING_WHITESPACE)

  ELSEIF(APPLE OR FREEBSD)
    EXEC_PROGRAM(stat ARGS -f '%z' ${FILE_NAME} OUTPUT_VARIABLE RESULT)
  ELSE()
    EXEC_PROGRAM(stat ARGS -c '%s' ${FILE_NAME} OUTPUT_VARIABLE RESULT)
  ENDIF()
  SET(${OUTPUT_SIZE} ${RESULT} PARENT_SCOPE)
ENDFUNCTION()

SET(BOOST_MAJOR "1")
SET(BOOST_MINOR "76")
SET(BOOST_PATCH "0")
SET(BOOST_PACKAGE_NAME "boost_${BOOST_MAJOR}_${BOOST_MINOR}_${BOOST_PATCH}")
SET(BOOST_TARBALL "${BOOST_PACKAGE_NAME}.tar.gz")
SET(BOOST_DOWNLOAD_URL
  "https://boostorg.jfrog.io/artifactory/main/release/${BOOST_MAJOR}.${BOOST_MINOR}.${BOOST_PATCH}/source/${BOOST_TARBALL}"
  )

MACRO(RESET_BOOST_VARIABLES)
  UNSET(BOOST_INCLUDE_DIR)
  UNSET(BOOST_INCLUDE_DIR CACHE)
  UNSET(LOCAL_BOOST_DIR)
  UNSET(LOCAL_BOOST_DIR CACHE)
  UNSET(LOCAL_BOOST_ZIP)
  UNSET(LOCAL_BOOST_ZIP CACHE)
ENDMACRO()

MACRO(ECHO_BOOST_VARIABLES)
  MESSAGE(STATUS "BOOST_INCLUDE_DIR ${BOOST_INCLUDE_DIR}")
  MESSAGE(STATUS "LOCAL_BOOST_DIR ${LOCAL_BOOST_DIR}")
  MESSAGE(STATUS "LOCAL_BOOST_ZIP ${LOCAL_BOOST_ZIP}")
ENDMACRO()

MACRO(COULD_NOT_FIND_BOOST)
  ECHO_BOOST_VARIABLES()
  RESET_BOOST_VARIABLES()
  MESSAGE(STATUS "Could not find (the correct version of) boost.")
  MESSAGE(STATUS "wsrep-lib currently requires ${BOOST_PACKAGE_NAME}\n")
  MESSAGE(FATAL_ERROR
    "You can download it with -DDOWNLOAD_BOOST=1 -DWITH_BOOST=<directory>\n"
    "This CMake script will look for boost in <directory>. "
    "If it is not there, it will download and unpack it "
    "(in that directory) for you.\n"
    "You can also download boost manually, from ${BOOST_DOWNLOAD_URL}\n"
    "If you are inside a firewall, you may need to use an https proxy:\n"
    "export https_proxy=http://example.com:80\n"
    )
ENDMACRO()

# Pick value from environment if not set on command line.
IF(DEFINED ENV{WITH_BOOST} AND NOT DEFINED WITH_BOOST)
  SET(WITH_BOOST "$ENV{WITH_BOOST}")
ENDIF()

# Pick value from environment if not set on command line.
IF(DEFINED ENV{BOOST_ROOT} AND NOT DEFINED WITH_BOOST)
  SET(WITH_BOOST "$ENV{BOOST_ROOT}")
ENDIF()

IF(WITH_BOOST AND WITH_BOOST STREQUAL "system")
  UNSET(WITH_BOOST)
  UNSET(WITH_BOOST CACHE)
ENDIF()

# Update the cache, to make it visible in cmake-gui.
SET(WITH_BOOST ${WITH_BOOST} CACHE PATH
  "Path to boost sources: a directory, or a tarball to be unzipped.")

# If the value of WITH_BOOST changes, we must unset all dependent variables:
IF(OLD_WITH_BOOST)
  IF(NOT "${OLD_WITH_BOOST}" STREQUAL "${WITH_BOOST}")
    RESET_BOOST_VARIABLES()
  ENDIF()
ENDIF()

SET(OLD_WITH_BOOST ${WITH_BOOST} CACHE INTERNAL
  "Previous version of WITH_BOOST" FORCE)

IF (WITH_BOOST)
  ## Did we get a full path name, including file name?
  IF (${WITH_BOOST} MATCHES ".*\\.tar.gz" OR ${WITH_BOOST} MATCHES ".*\\.zip")
    GET_FILENAME_COMPONENT(BOOST_DIR ${WITH_BOOST} PATH)
    GET_FILENAME_COMPONENT(BOOST_ZIP ${WITH_BOOST} NAME)
    FIND_FILE(LOCAL_BOOST_ZIP
              NAMES ${BOOST_ZIP}
              PATHS ${BOOST_DIR}
              NO_DEFAULT_PATH
             )
  ENDIF()
  ## Did we get a path name to the directory of the .tar.gz or .zip file?
  FIND_FILE(LOCAL_BOOST_ZIP
            NAMES "${BOOST_PACKAGE_NAME}.tar.gz" "${BOOST_PACKAGE_NAME}.zip"
            PATHS "${WITH_BOOST}"
            NO_DEFAULT_PATH
           )
  IF(LOCAL_BOOST_ZIP)
    MESSAGE(STATUS "Local boost zip: ${LOCAL_BOOST_ZIP}")
    GET_FILE_SIZE(${LOCAL_BOOST_ZIP} LOCAL_BOOST_ZIP_SIZE)
    IF(LOCAL_BOOST_ZIP_SIZE EQUAL 0)
      # A previous failed download has left an empty file, most likely the
      # user pressed Ctrl-C to kill a hanging connection due to missing vpn
      # proxy.  Remove it!
      MESSAGE("${LOCAL_BOOST_ZIP} is zero length. Deleting it.")
      FILE(REMOVE ${WITH_BOOST}/${BOOST_TARBALL})
      UNSET(LOCAL_BOOST_ZIP)
      UNSET(LOCAL_BOOST_ZIP CACHE)
    ENDIF()
    UNSET(LOCAL_BOOST_ZIP_ZERO_LENGTH)
  ENDIF()

  ## Did we get a path name to the directory of an unzipped version?
  FIND_FILE(LOCAL_BOOST_SOURCE
            NAMES "${BOOST_PACKAGE_NAME}"
            PATHS "${WITH_BOOST}"
            NO_DEFAULT_PATH
           )
  ## Did we get a path name to an unzippped version?
  FIND_PATH(LOCAL_BOOST_VER
            NAMES "boost/version.hpp"
            PATHS "${WITH_BOOST}"
            NO_DEFAULT_PATH
           )
  IF(LOCAL_BOOST_VER AND NOT LOCAL_BOOST_SOURCE)
    SET(LOCAL_BOOST_SOURCE ${WITH_BOOST})
    UNSET(LOCAL_BOOST_VER)
    UNSET(LOCAL_BOOST_VER CACHE)
  ENDIF()
  IF(LOCAL_BOOST_SOURCE)
    MESSAGE(STATUS "Local boost source: ${LOCAL_BOOST_SOURCE}")
  ENDIF()

  ## Did we get a path to compiled boost install?
  FIND_PATH(LOCAL_BOOST_LIB
            NAMES "lib"
            PATHS "${WITH_BOOST}"
            NO_DEFAULT_PATH
           )
  IF(LOCAL_BOOST_LIB)
    SET(LOCAL_BOOST_BUILD ${WITH_BOOST})
    UNSET(LOCAL_BOOST_LIB)
    UNSET(LOCAL_BOOST_LIB CACHE)
  ENDIF()
ENDIF()

# There is a similar option in unittest/gunit.
# But the boost tarball is much bigger, so we have a separate option.
OPTION(DOWNLOAD_BOOST "Download boost from sourceforge." OFF)
SET(DOWNLOAD_BOOST_TIMEOUT 600 CACHE STRING
  "Timeout in seconds when downloading boost.")

# If we could not find it, then maybe download it.
IF(WITH_BOOST AND NOT LOCAL_BOOST_ZIP AND NOT LOCAL_BOOST_SOURCE AND NOT LOCAL_BOOST_BUILD)
  IF(NOT DOWNLOAD_BOOST)
    MESSAGE(STATUS "WITH_BOOST=${WITH_BOOST}")
    COULD_NOT_FIND_BOOST()
  ENDIF()
  # Download the tarball
  MESSAGE(STATUS "Downloading ${BOOST_TARBALL} to ${WITH_BOOST}")
  FILE(DOWNLOAD ${BOOST_DOWNLOAD_URL}
    ${WITH_BOOST}/${BOOST_TARBALL}
    TIMEOUT ${DOWNLOAD_BOOST_TIMEOUT}
    STATUS ERR
    SHOW_PROGRESS
  )
  IF(ERR EQUAL 0)
    SET(LOCAL_BOOST_ZIP "${WITH_BOOST}/${BOOST_TARBALL}")
    SET(LOCAL_BOOST_ZIP "${WITH_BOOST}/${BOOST_TARBALL}" CACHE INTERNAL "")
  ELSE()
    MESSAGE(STATUS "Download failed, error: ${ERR}")
    # A failed DOWNLOAD leaves an empty file, remove it
    FILE(REMOVE ${WITH_BOOST}/${BOOST_TARBALL})
    # STATUS returns a list of length 2
    LIST(GET ERR 0 NUMERIC_RETURN)
    IF(NUMERIC_RETURN EQUAL 28)
      MESSAGE(FATAL_ERROR
        "You can try downloading ${BOOST_DOWNLOAD_URL} manually"
        " using curl/wget or a similar tool,"
        " or increase the value of DOWNLOAD_BOOST_TIMEOUT"
        " (which is now ${DOWNLOAD_BOOST_TIMEOUT} seconds)"
      )
    ENDIF()
    MESSAGE(FATAL_ERROR
      "You can try downloading ${BOOST_DOWNLOAD_URL} manually"
      " using curl/wget or a similar tool"
      )
  ENDIF()
ENDIF()

IF(LOCAL_BOOST_ZIP AND NOT LOCAL_BOOST_SOURCE AND NOT LOCAL_BOOST_BUILD)
  GET_FILENAME_COMPONENT(LOCAL_BOOST_DIR ${LOCAL_BOOST_ZIP} PATH)
  IF(NOT EXISTS "${LOCAL_BOOST_DIR}/${BOOST_PACKAGE_NAME}")
    GET_FILENAME_COMPONENT(BOOST_TARBALL ${LOCAL_BOOST_ZIP} NAME)
    MESSAGE(STATUS "cd ${LOCAL_BOOST_DIR}; tar xfz ${BOOST_TARBALL}")
    EXECUTE_PROCESS(
      COMMAND ${CMAKE_COMMAND} -E tar xfz "${BOOST_TARBALL}"
      WORKING_DIRECTORY "${LOCAL_BOOST_DIR}"
      RESULT_VARIABLE tar_result
      )
    IF (tar_result MATCHES 0)
      SET(BOOST_FOUND 1 CACHE INTERNAL "")
      SET(LOCAL_BOOST_SOURCE "${LOCAL_BOOST_DIR}/${BOOST_PACKAGE_NAME}")
    ELSE()
      MESSAGE(STATUS "WITH_BOOST ${WITH_BOOST}.")
      MESSAGE(STATUS "Failed to extract files.\n"
        "   Please try downloading and extracting yourself.\n"
        "   The url is: ${BOOST_DOWNLOAD_URL}")
      MESSAGE(FATAL_ERROR "Giving up.")
    ENDIF()
  ENDIF()
ENDIF()

IF (LOCAL_BOOST_SOURCE AND NOT LOCAL_BOOST_BUILD)
  GET_FILENAME_COMPONENT(LOCAL_BOOST_BUILD ${LOCAL_BOOST_SOURCE} PATH)
  SET(BOOST_TO_BUILD "thread,filesystem,program_options,test,json")
  MESSAGE(STATUS "Executing Boost configure")
  EXECUTE_PROCESS(
    COMMAND ./bootstrap.sh --prefix=${LOCAL_BOOST_BUILD} --with-libraries=${BOOST_TO_BUILD}
    WORKING_DIRECTORY "${LOCAL_BOOST_SOURCE}"
    RESULT_VARIABLE RES
    ERROR_VARIABLE ERR
    OUTPUT_FILE "bootstrap.log"
    ERROR_FILE "bootstrap.err"
    COMMAND_ECHO STDOUT
    )
  IF (NOT RES MATCHES 0)
    MESSAGE(FATAL_ERROR "Boost configure failed: ${ERR}. Logs at ${LOCAL_BOOST_SOURCE}/bootstrap.log, ${LOCAL_BOOST_SOURCE}/bootstrap.err")
  ENDIF()
  MESSAGE(STATUS "Executing Boost build")
  EXECUTE_PROCESS(
    COMMAND ./b2 install -q
    WORKING_DIRECTORY "${LOCAL_BOOST_SOURCE}"
    RESULT_VARIABLE RES
    ERROR_VARIABLE ERR
    OUTPUT_FILE "b2.log"
    ERROR_FILE "b2.err"
    COMMAND_ECHO STDOUT
    )
  IF (NOT RES MATCHES 0)
    MESSAGE(FATAL_ERROR "Boost build failed: ${ERR}.  Logs at ${LOCAL_BOOST_SOURCE}/b2.log, ${LOCAL_BOOST_SOURCE}/b2.err")
  ENDIF()
  # remove the source directory so save space
  FILE(REMOVE_RECURSE ${LOCAL_BOOST_SOURCE})
ENDIF()

# Search for the version file, first in LOCAL_BOOST_DIR or WITH_BOOST
MESSAGE(STATUS
  "Looking for boost/version.hpp in ${LOCAL_BOOST_BUILD}/include")
FIND_PATH(BOOST_INCLUDE_DIR
  NAMES "boost/version.hpp"
  NO_DEFAULT_PATH
  PATHS ${LOCAL_BOOST_BUILD}/include
)
# Then search in standard places (if not found above).
FIND_PATH(BOOST_INCLUDE_DIR
  NAMES "boost/version.hpp"
)

IF(NOT BOOST_INCLUDE_DIR)
  MESSAGE(STATUS
    "Looked for boost/version.hpp in ${LOCAL_BOOST_BUILD}/include and default locations")
  COULD_NOT_FIND_BOOST()
ELSE()
  MESSAGE(STATUS "Found ${BOOST_INCLUDE_DIR}/boost/version.hpp ")
ENDIF()

# Verify version number. Version information looks like:
# //  BOOST_VERSION % 100 is the patch level
# //  BOOST_VERSION / 100 % 1000 is the minor version
# //  BOOST_VERSION / 100000 is the major version
# #define BOOST_VERSION 107300
FILE(STRINGS "${BOOST_INCLUDE_DIR}/boost/version.hpp"
  BOOST_VERSION_NUMBER
  REGEX "^#define[\t ]+BOOST_VERSION[\t ][0-9]+.*"
)
STRING(REGEX REPLACE
  "^.*BOOST_VERSION[\t ]([0-9][0-9])([0-9][0-9])([0-9][0-9]).*$" "\\1"
  BOOST_MAJOR_VERSION "${BOOST_VERSION_NUMBER}")
STRING(REGEX REPLACE
  "^.*BOOST_VERSION[\t ]([0-9][0-9])([0-9][0-9])([0-9][0-9]).*$" "\\2"
  BOOST_MINOR_VERSION "${BOOST_VERSION_NUMBER}")

MESSAGE(STATUS "BOOST_VERSION_NUMBER is ${BOOST_VERSION_NUMBER}")

MESSAGE(STATUS "BOOST_INCLUDE_DIR ${BOOST_INCLUDE_DIR}")

# Boost gets confused about language support with Clang 7 + MSVC 15.9
IF(WIN32_CLANG)
  ADD_DEFINITIONS(-DBOOST_NO_CXX17_HDR_STRING_VIEW)
ENDIF()

IF(LOCAL_BOOST_BUILD OR LOCAL_BOOST_ZIP)
  SET(USING_LOCAL_BOOST 1)
ELSE()
  SET(USING_SYSTEM_BOOST 1)
ENDIF()
