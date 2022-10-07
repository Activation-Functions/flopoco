
#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "flopoco/Trigs/Fix2DNorm.hpp"
//#include "FixFunctions/FixFunctionByTable.hpp"
// #include "FixFunctions/BipartiteTable.hpp"
// #include "FixFunctions/FixFunctionByPiecewisePoly.hpp"


using namespace std;
namespace flopoco{



	Fix2DNorm::Fix2DNorm(OperatorPtr parentOp, Target* target_, int msb_, int lsb_) :
		Operator(parentOp, target_), msb(msb_), lsb(lsb_)
	{
		srcFileName="Fix2DNorm";
		setCopyrightString ( "Florent de Dinechin (2015-...)" );
		//		useNumericStd_Unsigned();

		ostringstream name;
		name << "Fix2DNorm_" << msb << "_" << lsb;
		setNameWithFreqAndUID( name.str() );

		addFixInput ("X",  false, msb, lsb);
		addFixInput ("Y",  false, msb, lsb);
		addFixOutput ("R",  false, msb, lsb, 2);
	};


	Fix2DNorm::~Fix2DNorm(){
	};



	
	OperatorPtr Fix2DNorm::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {		
		int msb,lsb, method;
		ui.parseInt(args, "msb", &msb);
		ui.parseInt(args, "lsb", &lsb);
		ui.parseInt(args, "method", &method);
		//select the method
		return new Fix2DNorm(parentOp, target, msb, lsb);
			
	}

	template <>
	OperatorDescription<Fix2DNorm> op_descriptor<Fix2DNorm> {
	    "Fix2DNorm", // name
	    "Computes sqrt(x*x+y*y)",
	    "CompositeFixPoint",
	    "", // seeAlso
	    "msb(int): weight of the MSB of both inputs and outputs; lsb(int): weight of the LSB of both inputs and outputs; \
         method(int)=-1: technique to use, -1 selects a sensible default",
	    ""};
}

