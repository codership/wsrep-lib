
#include "logger.hpp"

#include <iostream>

std::ostream& trrep::log::os_ = std::cout;
static trrep::default_mutex log_mutex_;
trrep::mutex& trrep::log::mutex_ = log_mutex_;
