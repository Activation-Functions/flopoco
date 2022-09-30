#ifndef _FIXFUNCTIONEMULATOR_HPP_
#define _FIXFUNCTIONEMULATOR_HPP_

#include <string>
#include <iostream>

#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/TestBenches/TestCase.hpp"

namespace flopoco {
 void emulate_fixfunction(FixFunction const & fixfunc, TestCase * tc, bool correctlyRounded=false);
}
#endif // _FIXFUNCTIONEMULATOR_HPP_
