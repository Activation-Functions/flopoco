#ifndef POSIT2POSIT_HPP
#define POSIT2POSIT_HPP

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"


namespace flopoco {

	// new operator class declaration
	class Posit2Posit : public Operator {

	public:
	
	  Posit2Posit(Target* target, Operator* parentOp, int widthI, int wES);

		// destructor
		//~Posit2Posit() {};

		void emulate(TestCase * tc);

		
		void buildStandardTestCases(TestCaseList* tcl);


		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
	private:
	  int widthI_;
	  int wES_;
    
	};


}//namespace
#endif