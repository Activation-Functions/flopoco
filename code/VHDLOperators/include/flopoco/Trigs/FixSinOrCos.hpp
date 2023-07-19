#ifndef FIXEDPOINTSINORCOS_HPP
#define FIXEDPOINTSINORCOS_HPP

#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"
#include "flopoco/FixFunctions/FixFunctionByPiecewisePoly.hpp"

namespace flopoco{ 


	class FixSinOrCos : public Operator {
	  
	  public:
		int w;
		int degree;
		mpfr_t scale;
	  
		// constructor, defined there with two parameters (default value 0 for each)
		FixSinOrCos(OperatorPtr parentOp, Target* target, int w, int degree);

		// destructor
		~FixSinOrCos();
		
		//????		void changeName(std::string operatorName);


		// Below all the functions needed to test the operator
		/* the emulate function is used to simulate in software the operator
		  in order to compare this result with those outputed by the vhdl opertator */
		void emulate(TestCase * tc);

		/* function used to create Standard testCase defined by the developper */
		void buildStandardTestCases(TestCaseList* tcl);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		static TestList unitTest(int testLevel);
		
		/** Factory register method */ 
		static void registerFactory();

	};

	
}

#endif
