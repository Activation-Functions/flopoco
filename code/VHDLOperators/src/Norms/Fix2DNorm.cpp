#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "flopoco/Norms/Fix2DNorm.hpp"
#include "flopoco/Norms/Fix2DNormCORDIC.hpp"
#include "flopoco/Norms/Fix2DNormLUT.hpp"

using namespace std;
namespace flopoco {

	Fix2DNorm::Fix2DNorm(OperatorPtr parentOp, Target* target_, int lsbIn_, int lsbOut_) :
		Operator(parentOp, target_), lsbIn(lsbIn_), lsbOut(lsbOut_)
	{
		int wIn = getWIn();
		int wOut = getWOut();				

		addInput  ("X", wIn);
		addInput  ("Y", wIn);
		addOutput ("R", wOut, 2);
	}


	void Fix2DNorm::emulate (TestCase * tc) {
		int wIn = getWIn();
		int wOut = getWOut();
		mpfr_t x, y, r;
		mpfr_init2 (x, 10*wIn);
		mpfr_init2 (y, 10*wIn);
		mpfr_init2 (r, 10*wOut);
  
		mpz_class rz;

		/* Get I/O values */
		mpz_class svX = tc->getInputValue ("X");
		mpz_class svY = tc->getInputValue ("Y");
		
		mpfr_set_z (x, svX.get_mpz_t(), MPFR_RNDN); // exact
		mpfr_mul_2si (x, x, lsbIn, MPFR_RNDN);      // exact
		mpfr_set_z (y, svY.get_mpz_t(), MPFR_RNDN); // exact
	        mpfr_mul_2si (y, y, lsbIn, MPFR_RNDN);      // exact
		
		/* Euclidean norm */
		mpfr_hypot(r, x, y, MPFR_RNDN);
		
		/* Convert r to fix point */
		mpfr_add_d (r, r, 6.0, MPFR_RNDN);
		mpfr_mul_2si (r, r, -lsbOut, MPFR_RNDN); // exact scaling

		mpz_class mask = (mpz_class(1) << wOut) - 1;
		
		/* Rounding down */
		mpfr_get_z (rz.get_mpz_t(), r, MPFR_RNDD); // there can be a real rounding here
		rz -= mpz_class(6) << -lsbOut;
		rz &= mask;
		tc->addExpectedOutput ("R", rz);

		/* Rounding up */
		mpfr_get_z (rz.get_mpz_t(), r, MPFR_RNDU); // there can be a real rounding here
		rz -= mpz_class(6) << -lsbOut;
		rz &= mask;
		tc->addExpectedOutput ("R", rz);

		/* clean up */
		mpfr_clears (x, y, r, NULL);
	}

	void Fix2DNorm::buildStandardTestCases (TestCaseList * tcl) {
		TestCase* tc;
		
		/* N(0, 0) = 0 */
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(0));
		tc -> addInput ("Y", mpz_class(0));
		emulate(tc);
		tcl->add(tc);

		/* Define 0.999_ */
		int wIn = msbIn - lsbIn + 1;
		mpz_class max = (mpz_class(1) << wIn) - mpz_class(1);

		/* N(0.999_, 0) = 0.999_ */
		tc = new TestCase (this);
		tc -> addInput ("X", max);
		tc -> addInput ("Y", mpz_class(0));
		emulate(tc);
		tcl->add(tc);

		/* N(0, 0.999_) = 0.999_ */
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(0));
		tc -> addInput ("Y", max);
		emulate(tc);
		tcl->add(tc);
		
		/* N(0.999_, 0.999_) ~ sqrt(2) */
		tc = new TestCase (this);
		tc -> addInput ("X", max);
		tc -> addInput ("Y", max);
		emulate(tc);
		tcl->add(tc);
	}



	TestList Fix2DNorm::unitTest(int index)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
		if(index==-1) 
		{ // The unit tests
			int method=0;
			for(int wIn=4; wIn<33; wIn+=(wIn<16?1:3)) {
					paramList.push_back(make_pair("lsbIn",to_string(-wIn)));
					paramList.push_back(make_pair("lsbOut",to_string(-wIn)));
					paramList.push_back(make_pair("method",to_string(method)));
					testStateList.push_back(paramList);
					paramList.clear();
					paramList.push_back(make_pair("lsbIn",to_string(-wIn)));
					paramList.push_back(make_pair("lsbOut",to_string(-wIn-1)));
					paramList.push_back(make_pair("method",to_string(method)));
					testStateList.push_back(paramList);
					paramList.clear();
			}
			method=1;
			for(int wIn=4; wIn<12; wIn++) {
					paramList.push_back(make_pair("lsbIn",to_string(-wIn)));
					paramList.push_back(make_pair("lsbOut",to_string(-wIn)));
					paramList.push_back(make_pair("method",to_string(method)));
					testStateList.push_back(paramList);
					paramList.clear();
					paramList.push_back(make_pair("lsbIn",to_string(-wIn)));
					paramList.push_back(make_pair("lsbOut",to_string(-wIn-1)));
					paramList.push_back(make_pair("method",to_string(method)));
					testStateList.push_back(paramList);
					paramList.clear();
			}
		}
		else     		{
				// finite number of random test computed out of index
				// TODO
		}	
		cerr << "************* "<< testStateList.size() <<endl;
		return testStateList;
	}

	OperatorPtr Fix2DNorm::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int lsbIn, lsbOut, method;
		ui.parseInt(args, "lsbIn",	&lsbIn);
		ui.parseInt(args, "lsbOut", &lsbOut);
		ui.parseInt(args, "method", &method);
		//select the method
		switch (method) {
		case 1:
			      return new Fix2DNormLUT(parentOp, target, lsbIn, lsbOut);
		        default:
			      return new Fix2DNormCORDIC(parentOp, target, lsbIn, lsbOut);
		}
	}


	
	template <>
	const OperatorDescription<Fix2DNorm> op_descriptor<Fix2DNorm> {
		"Fix2DNorm", // name
			"Computes sqrt(x*x+y*y) in fixed point.",
			"CompositeFixPoint",
			"", // seeAlso
			"lsbIn(int): weight of the LSB of input;"
			"lsbOut(int): weight of the LSB of output;"
			"method(int)=0: technique to use. Can be : 0 for CORDIC or 1 for LUT",
			""
			};
}

