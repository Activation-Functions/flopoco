#include <iostream>
#include <ostream>

#include "flopoco/report.hpp"

namespace {
    flopoco::LogLevel loglevel{flopoco::MESSAGE};
}

namespace flopoco {

  bool is_log_lvl_enabled(LogLevel lvl) {
    return lvl <= loglevel;
  }
}