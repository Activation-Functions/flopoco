#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "flopoco/FixFilters/FixHalfSine.hpp"

using namespace std;

namespace flopoco{


	FixHalfSine::FixHalfSine(OperatorPtr parentOp_, Target* target_, int lsbIn_, int lsbOut_, int N_) :
		FixFIR(parentOp_, target_, lsbIn_, lsbOut_), N(N_)
	{
		srcFileName="FixHalfSine";
		
		ostringstream name;
		name << "FixHalfSine_" << -lsbIn  << "_" << -lsbOut  << "_" << N;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Louis Besème, Florent de Dinechin, Matei Istoan (2014-2018)");

		symmetry = 1; // this is a FIR attribute, Half-sine is a symmetric filter
		rescale=true; // also a FIR attribute
		// define the coefficients
		for (int i=1; i<2*N; i++) {
			ostringstream c;
			c <<  "sin(pi*" << i << "/" << 2*N << ")";
			coeff.push_back(c.str());
		}

		buildVHDL();
	};

	FixHalfSine::~FixHalfSine(){}

	OperatorPtr FixHalfSine::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int lsbIn;
		ui.parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		ui.parseInt(args, "lsbOut", &lsbOut);
		int n;
		ui.parseStrictlyPositiveInt(args, "n", &n);
		OperatorPtr tmpOp = new FixHalfSine(parentOp, target, lsbIn, lsbOut, n);
		return tmpOp;
	}


	TestList FixHalfSine::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
    if(testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
			for(int lsbIn=-1; lsbIn>=-12; lsbIn-=1) {
				for(int lsbOut = min(lsbIn,-5); lsbOut>=-18; lsbOut-=3)	{
					for(int n = 3; n<8; n++)	{
						paramList.push_back(make_pair("lsbIn",to_string(lsbIn)));
						paramList.push_back(make_pair("lsbOut",to_string(lsbOut)));
						paramList.push_back(make_pair("n",to_string(n)));
						testStateList.push_back(paramList);
						paramList.clear();
					}					
				}
			}
		}
		else     
		{
				// finite number of random test computed out of testLevel
			// TODO
		}	
		return testStateList;
	}

	template <>
	const OperatorDescription<FixHalfSine> op_descriptor<FixHalfSine> {
	    "FixHalfSine", // name
	    "A generator of fixed-point Half-Sine filters, for inputs between "
	    "-1 and 1",
	    "FiltersEtc", // categories
	    "",
	    "lsbIn(int): position of the LSB of the input, e.g. -15 for a 16-bit signed input;\
											  lsbOut(int): position of the LSB of the output;\
                        n(int): filter order (number of taps will be 2n)",
	    "For more details, see <a "
	    "href=\"bib/flopoco.html#DinIstoMas2014-SOPCJR\">this "
	    "article</a>."};
}


