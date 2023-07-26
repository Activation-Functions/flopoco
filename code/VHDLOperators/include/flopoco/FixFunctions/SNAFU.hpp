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

#include <vector>
#include <sstream>

#include "flopoco/Operator.hpp"

namespace flopoco{

	/** A generator of Simple Neural Actication Function Units (SNAFU) 
			without reinventing the wheel
	*/

  class SNAFU : public Operator
  {
  public:
		
		/** The constructor.will actually never be called, so no parameters
     */
		
		SNAFU(OperatorPtr parentOp, Target* target);
		
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

	};
	
}
