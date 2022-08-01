/*
  FixFunction object for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Aric team at Ecole Normale Superieure de Lyon
	then by the Socrate team at INSA de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

*/

#include <sstream>

#include "FixFunctions/FixFunctionEmulator.hpp"

namespace flopoco{
void emulate_fixfunction(FixFunction const & fixfunc, TestCase * tc, bool correctlyRounded) {
	mpz_class x = tc->getInputValue("X");
	mpz_class rNorD,ru;
	fixfunc.eval(x,rNorD,ru,correctlyRounded);
	//cerr << " x=" << x << " -> " << rNorD << " " << ru << endl; // for debugging
	tc->addExpectedOutput("Y", rNorD);
	if(!correctlyRounded)
		tc->addExpectedOutput("Y", ru);
}
} //namespace
