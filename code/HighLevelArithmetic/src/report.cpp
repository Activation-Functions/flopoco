#include <filesystem>
#include <iostream>
#include <ostream>

#include "flopoco/report.hpp"

namespace fs = std::filesystem;

namespace {
    flopoco::LogLevel loglevel{flopoco::MESSAGE};
}

namespace flopoco {

  bool is_log_lvl_enabled(LogLevel lvl) {
    return lvl <= loglevel;
  }


  void set_log_lvl(LogLevel lvl) {
    loglevel = lvl;
  }

  LogLevel get_log_lvl() {
    return loglevel;
  }

    void report (LogLevel lvl, std::string_view message, std::string_view filename, int line, std::string_view funcname) {
    if (is_log_lvl_enabled(lvl)) {
        std::ostream& out = (lvl < 0) ? std::cerr : std::cout;
				if(lvl>=3)	{
					out << "" << filename << ":" << line <<" (" << funcname << "): ";
				}
				else {
					out << "" << fs::path{filename}.filename() << ": ";
				}
				out << message << std::endl;
    }
  }
}