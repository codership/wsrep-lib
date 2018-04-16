#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#



add_library(trrep
  provider.cpp
  client_context.cpp
  transaction_context.cpp)

add_executable(trrep_test
  transaction_context_test.cpp
  trrep_test.cpp
  )
target_link_libraries(trrep_test trrep)
add_test(NAME trrep_test
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/trrep_test
  )

