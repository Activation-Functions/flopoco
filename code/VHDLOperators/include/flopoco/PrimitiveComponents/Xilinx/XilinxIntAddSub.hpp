#ifndef Xilinx_GenericAddSub_H
#define Xilinx_GenericAddSub_H

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco {

	// new operator class declaration
	class XilinxIntAddSub : public Operator {
		const int wIn_;
		const int signs_;
		const bool dss_;
	public:
		// definition of some function for the operator

		// constructor, defined there with two parameters (default value 0 for each)
		XilinxIntAddSub(Operator* parentOp, Target *target, int wIn = 10, bool dss = false );
		XilinxIntAddSub(Operator* parentOp, Target *target, int wIn = 10, int fixed_signs = -1 );

		void build_normal( Target *target, int wIn, bool configurable);

		void build_with_dss( Target *target, int wIn );

		void build_with_fixed_sign( Target *target, int wIn, int fixed_signs );

		// destructor
		~XilinxIntAddSub();
		
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
	
		virtual void emulate(TestCase *tc);
    static TestList unitTest(int index);
	};


}//namespace

#endif
