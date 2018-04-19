#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#



add_library(trrep
  provider.cpp
  client_context.cpp
  server_context.cpp
  transaction_context.cpp
  wsrep_provider_v26.cpp)
target_link_libraries(trrep wsrep pthread dl)

add_executable(trrep_test
  mock_client_context.cpp
  mock_utils.cpp
  server_context_test.cpp
  transaction_context_test.cpp
  trrep_test.cpp
  )
target_link_libraries(trrep_test trrep)
add_test(NAME trrep_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/trrep_test
  )

add_executable(dbms_simulator
  dbms_simulator.cpp)
target_link_libraries(dbms_simulator trrep ${Boost_PROGRAM_OPTIONS_LIBRARY})
set_property(TARGET dbms_simulator PROPERTY CXX_STANDARD 14)
