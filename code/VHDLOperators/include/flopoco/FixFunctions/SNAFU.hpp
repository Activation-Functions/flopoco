/*

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA,
  2008-2014.
  All rights reserved.

*/
#pragma once

#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/Operator.hpp"

#include <vector>

namespace flopoco
{

  /** A generator of Simple Neural Actication Function Units (SNAFU)
			without reinventing the wheel
	*/

  class SNAFU : public Operator {
    public:
    SNAFU(OperatorPtr parentOp, Target* target, string f, int wIn, int wOut, string method, double inputScale, int adhocCompression);

    void emulate(TestCase* tc);

    static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args, UserInterface& ui);
    private:
    int wIn;
    int wOut;
    double inputScale;
    int adhocCompression;
    string sollyaDeltaFunction;  // when we use compression
    FixFunction* f;
    bool correctlyRounded;
  };

}  // namespace flopoco
