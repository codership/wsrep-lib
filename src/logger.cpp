
#include "wsrep/logger.hpp"

#include <iostream>

std::ostream& wsrep::log::os_ = std::cout;
static wsrep::default_mutex log_mutex_;
wsrep::mutex& wsrep::log::mutex_ = log_mutex_;
