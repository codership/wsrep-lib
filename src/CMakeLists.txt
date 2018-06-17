#
# Copyright (C) 2018 Codership Oy <info@codership.com>
#

add_library(wsrep-lib
  client_state.cpp
  id.cpp
  logger.cpp
  provider.cpp
  server_context.cpp
  transaction.cpp
  wsrep_provider_v26.cpp)
target_link_libraries(wsrep-lib wsrep_api_v26 pthread dl)
