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

    void report (LogLevel lvl, std::string const & message, std::string const & filename, int line, std::string const & funcname) {
    if (is_log_lvl_enabled(lvl)) {
        std::ostream& out = (lvl < 0) ? std::cerr : std::cerr;
				if(lvl>=3)	{
					// the full file name annoys me
					//					out << "" << filename << ":" << line <<" (" << funcname << "): ";
					out << "" << fs::path{filename}.filename() << ":" << line <<" (" << funcname << "): ";
				}
				else {
					out << "" << fs::path{filename}.filename() << ": ";
				}
				out << message << std::endl;
    }
  }
}
