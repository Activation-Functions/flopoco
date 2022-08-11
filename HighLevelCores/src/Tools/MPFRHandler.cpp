#include "Tools/MPFRHandler.hpp"

namespace flopoco {

MPFRHandler::MPFRHandler(size_t precision) { mpfr_init2(managed, precision); }
MPFRHandler::~MPFRHandler() {
  mpfr_clear(managed);
}
} // namespace flopoco
