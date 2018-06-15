#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_executable(dbms_simulator
  db_client.cpp
  db_server.cpp
  db_storage_engine.cpp

  #  db_client_service.cpp
#  dbms_simulator.cpp
)
target_link_libraries(dbms_simulator wsrep-lib ${Boost_PROGRAM_OPTIONS_LIBRARY} ${Boost_SYSTEM_LIBRARY} ${Boost_FILESYSTEM_LIBRARY} ${Boost_THREAD_LIBRARY})
set_property(TARGET dbms_simulator PROPERTY CXX_STANDARD 14)
