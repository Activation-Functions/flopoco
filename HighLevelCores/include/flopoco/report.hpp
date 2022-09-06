#ifndef REPORT_HPP
#define REPORT_HPP

#include <functional>
#include <iostream>
#include <ostream>
#include <source_location>
#include <sstream>
#include <string_view>

namespace flopoco {
  enum LogLevel {
    CRITICAL = -2,
    ERROR = -1,
    MESSAGE = 0,
    DETAIL = 1,
    VERBOSE = 2,
    DEBUG = 3,
    FULL = 4
  };

  bool is_log_lvl_enabled(LogLevel lvl);

  /**
   * @brief Perform an action based on log level
   *        Useful for using external lib printing facilities (e.g. sollya) when incompatible with 
   *        standard stream mechanism
   * 
   * @param lvl 
   * @param action 
   */
  void external_log(LogLevel lvl, auto action){
    if (is_log_lvl_enabled(lvl)){
        action();
    }
  }

  void report(LogLevel lvl, std::string_view message, std::source_location location);
}

#define REPORT(level, stream) { \
    std::stringstream s; \
    auto sl = std::source_location::current();\
    s << stream; \
    flopoco::report(level, s.str(), sl); \
}

#endif