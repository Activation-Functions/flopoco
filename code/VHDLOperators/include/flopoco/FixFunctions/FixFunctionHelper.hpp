#ifndef _FIXFUNCTIONEMULATOR_HPP_
#define _FIXFUNCTIONEMULATOR_HPP_

#include <string>
#include <iostream>

#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/TestBenches/TestCase.hpp"
#include "flopoco/AutoTest/AutoTest.hpp"

namespace flopoco {

 void emulate_fixfunction(FixFunction const & fixfunc, TestCase * tc, bool correctlyRounded=false);
 
 TestList generateFixFunctionUnitTest(int testLevel, int methodSize);
}
#endif // _FIXFUNCTIONEMULATOR_HPP_
