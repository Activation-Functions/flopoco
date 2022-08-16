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

  void report(LogLevel lvl, std::string_view message, std::source_location location) {
    if (is_log_lvl_enabled(lvl)) {
        std::ostream& out = (lvl < 0) ? std::cerr : std::cout;
        out << "> (" << location.file_name() << ":" << location.line() <<" ("<< location.function_name()<<")): " << message << std::endl;
    }
  }
}