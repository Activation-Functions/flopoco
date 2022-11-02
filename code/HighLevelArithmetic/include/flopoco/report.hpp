#ifndef REPORT_HPP
#define REPORT_HPP

#include <functional>
#include <iostream>
#include <ostream>
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

  void set_log_lvl(LogLevel lvl);

  /**
   * @brief Perform an action based on log level
   *        Useful for using external lib printing facilities (e.g. sollya) when incompatible with 
   *        standard stream mechanism
   * 
   * @param lvl 
   * @param action 
   */
  template<typename T>
  void external_log(LogLevel lvl, T action){
    if (is_log_lvl_enabled(lvl)){
        action();
    }
  };

  LogLevel get_log_lvl();

  void report(LogLevel lvl, std::string const & message, std::string const & filename, int line, std::string const & funcname);
}

#define REPORT(level, stream) { \
    std::stringstream __OUT_STREAM__; \
    __OUT_STREAM__ << "" << stream; \
    flopoco::report(level, __OUT_STREAM__.str(), __FILE__, __LINE__, __func__); \
}

#endif
