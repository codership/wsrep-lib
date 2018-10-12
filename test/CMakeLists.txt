#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_executable(wsrep-lib_test
  mock_client_state.cpp
  mock_high_priority_service.cpp
  mock_storage_service.cpp
  test_utils.cpp
  id_test.cpp
  server_context_test.cpp
  transaction_test.cpp
  transaction_test_2pc.cpp
  wsrep-lib_test.cpp
  )
target_link_libraries(wsrep-lib_test wsrep-lib)
add_test(NAME wsrep-lib_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/wsrep-lib_test
  )

if (WSREP_LIB_WITH_AUTO_TEST)
  set(UNIT_TEST wsrep-lib_test)
  add_custom_command(
    TARGET ${UNIT_TEST}
    COMMENT "Run tests"
    POST_BUILD
    COMMAND ${UNIT_TEST}
    )
endif()
