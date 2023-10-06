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

  FixFIRTransposed::FixFIRTransposed(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut, vector<string> coeff, int symmetry, bool rescale): Operator(parentOp, target), lsbIn(lsbIn), lsbOut(lsbOut)
	{

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
		int lsbIn;
		ui.parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		ui.parseInt(args, "lsbOut", &lsbOut);
		int symmetry;
		ui.parseInt(args, "symmetry", &symmetry);
		bool rescale;
		ui.parseBoolean(args, "rescale", &rescale);
		vector<string> coeffs;
		ui.parseColonSeparatedStringList(args, "coeff", &coeffs);

		OperatorPtr tmpOp = new FixFIRTransposed(parentOp, target, lsbIn, lsbOut, coeffs, symmetry, rescale);

		return tmpOp;
	}

	template <>
	const OperatorDescription<FixFIRTransposed> op_descriptor<FixFIRTransposed> {
	    "FixFIRTransposed", // name
	    "A fix-point Finite Impulse Filter generator in transposed form using shif-and-add.",
	    "FiltersEtc", // categories
	    "",
	    "lsbIn(int): integer size in bits;\
											 lsbOut(int): integer size in bits;								\
           						 symmetry(int)=0: 0 for normal filter, 1 for symmetric, -1 for antisymmetric. If not 0, only the first half of the coeff list is used.; \
                       rescale(bool)=false: If true, divides all coefficients by 1/sum(|coeff|);\
                       coeff(string): colon-separated list of real coefficients using Sollya syntax. Example: coeff=\"1.234567890123:sin(3*pi/8)\"",
	    ""};
}

