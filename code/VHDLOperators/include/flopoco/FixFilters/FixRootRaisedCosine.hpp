#ifndef FIXROOTRAISEDCOSINE_HPP
#define FIXROOTRAISEDCOSINE_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "flopoco/FixFilters/FixFIR.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco{

	class FixRootRaisedCosine : public FixFIR
	{
	public:

		FixRootRaisedCosine(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut, int N, double alpha_);

		virtual ~FixRootRaisedCosine();

		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
		static TestList unitTest(int index);

	private: 
		int N; /* FixFIR::n = 2*N+1 */
		double alpha;
	};

}


#endif
