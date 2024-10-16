#ifndef FIXHALFSINE_HPP
#define FIXHALFSINE_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "flopoco/FixFilters/FixFIR.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco{

	class FixHalfSine : public FixFIR
	{
	public:

		FixHalfSine(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut, int N);

		virtual ~FixHalfSine();

		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
		static TestList unitTest(int testLevel);

	private: 
		int N; /* FixFIR::n = 2*N */
	};

}


#endif
