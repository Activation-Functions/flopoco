#include <iostream>
#include <iomanip>
#include <sstream>

#include "flopoco/UserInterface.hpp"
#include "gmp.h"
#include "mpfr.h"
#include "sollya.h"

#include "flopoco/FixFilters/FixFIRTransposed.hpp"

#include "flopoco/ShiftReg.hpp"

using namespace std;

namespace flopoco {

	const int veryLargePrec = 6400;  /*6400 bits should be enough for anybody */

  FixFIRTransposed::FixFIRTransposed(OperatorPtr parentOp, Target* target, int wIn, vector<string> coeff): Operator(parentOp, target), wIn(wIn)
	{
    for(auto c : coeff)
    {
      REPORT(LogLevel::MESSAGE,"Got coefficient " << c);
    }
	};


	void FixFIRTransposed::emulate(TestCase * tc){
		mpz_class sx = tc->getInputValue("X"); 		// get the input bit vector as an integer
		xHistory[currentIndex] = sx;

		// Not completely optimal in terms of object copies...
		vector<mpz_class> inputs;
		for (int i=0; i< n; i++)	{
			sx = xHistory[(currentIndex+n-i)%n];
			inputs.push_back(sx);
		}
//		pair<mpz_class,mpz_class> results = refFixSOPC-> computeSOPCForEmulate(inputs);

//		tc->addExpectedOutput ("R", results.first);
//		tc->addExpectedOutput ("R", results.second);
		currentIndex=(currentIndex+1)%n; //  circular buffer to store the inputs

	};

	OperatorPtr FixFIRTransposed::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wIn;
		ui.parseInt(args, "wIn", &wIn);
		vector<string> coeffs;
		ui.parseColonSeparatedStringList(args, "coeff", &coeffs);

		OperatorPtr tmpOp = new FixFIRTransposed(parentOp, target, wIn, coeffs);

		return tmpOp;
	}

	template <>
	const OperatorDescription<FixFIRTransposed> op_descriptor<FixFIRTransposed> {
	    "FixFIRTransposed", // name
	    "A fix-point Finite Impulse Filter generator in transposed form using shif-and-add.",
	    "FiltersEtc", // categories
	    "",
	    "wIn(int): input word size in bits;\
                  coeff(string): colon-separated list of integer coefficients. Example: coeff=\"123:321:123\"",
	    ""};
}

