#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#



add_library(wsrep-lib
  client_context.cpp
  id.cpp
  logger.cpp
  provider.cpp
  server_context.cpp
  transaction_context.cpp
  wsrep_provider_v26.cpp)
target_link_libraries(wsrep-lib wsrep_api_v26 pthread dl)

add_executable(wsrep-lib_test
  mock_client_context.cpp
  test_utils.cpp
  server_context_test.cpp
  transaction_context_test.cpp
  wsrep-lib_test.cpp
  )
target_link_libraries(wsrep-lib_test wsrep-lib)
add_test(NAME wsrep-lib_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wsrep-lib_test
  )

if (WITH_AUTO_TEST)
  set(UNIT_TEST wsrep-lib_test)
  add_custom_command(
    TARGET ${UNIT_TEST}
    COMMENT "Run tests"
    POST_BUILD
    COMMAND ${UNIT_TEST}
    )
endif()

add_executable(dbms_simulator
  dbms_simulator.cpp)
target_link_libraries(dbms_simulator wsrep-lib ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})
set_property(TARGET dbms_simulator PROPERTY CXX_STANDARD 14)
