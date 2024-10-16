#ifdef HAVE_CONFIG_H
#include "config.h"
#endif


#include "flopoco/FixFilters/FixRootRaisedCosine.hpp"

using namespace std;

namespace flopoco{


	FixRootRaisedCosine::FixRootRaisedCosine(OperatorPtr parentOp_, Target* target_, int lsbIn_, int lsbOut_, int N_, double alpha_) :
		FixFIR(parentOp_, target_, lsbIn_, lsbOut_), N(N_)
	{
		srcFileName="FixRootRaisedCosine";
		
		ostringstream name;
		name << "FixRootRaisedCosine_lsbIn" << -lsbIn  << "_lsbOut" << -lsbOut  << "_N" << N << "_rolloff" << alpha;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Louis Besème, Florent de Dinechin, Matei Istoan (2014-2020)");

		// FROM THERE ON I AM NOT SURE
		// MPFR code for computing coeffs is  in Attic/FixRCF
		
		symmetry = -1; // this is a FIR attribute, Root-Raised Cosine is a symmetric filter
		rescale=true; // also a FIR attribute
		// define the coefficients

		THROWERROR("Not ported to the modern FloPoCo, sorry, ask nicely");
		buildVHDL();
	};

	FixRootRaisedCosine::~FixRootRaisedCosine(){}

	OperatorPtr FixRootRaisedCosine::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int lsbIn;
		ui.parseInt(args, "lsbIn", &lsbIn);
		int lsbOut;
		ui.parseInt(args, "lsbOut", &lsbOut);
		int n;
		ui.parseStrictlyPositiveInt(args, "n", &n);
		double alpha;
		ui.parseFloat(args, "alpha", &alpha);	
		OperatorPtr tmpOp = new FixRootRaisedCosine(parentOp, target, lsbIn, lsbOut, n, alpha);
		return tmpOp;
	}


	TestList FixRootRaisedCosine::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		
    if(testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
			for(int lsbIn=-3; lsbIn>=-12; lsbIn-=3) {
				for(int lsbOut = min(lsbIn,-5); lsbOut>=-18; lsbOut-=3)	{
					for(int n = 3; n<8; n++)	{
						for(double alpha=0; alpha <=1; alpha+=0.25)	{
							paramList.push_back(make_pair("lsbIn",to_string(lsbIn)));
							paramList.push_back(make_pair("lsbOut",to_string(lsbOut)));
							paramList.push_back(make_pair("n",to_string(n)));
							paramList.push_back(make_pair("alpha",to_string(alpha)));
							testStateList.push_back(paramList);
							paramList.clear();
						}					
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
	const OperatorDescription<FixRootRaisedCosine> op_descriptor<FixRootRaisedCosine> {
		"FixRootRaisedCosine", // name
		"A generator of fixed-point Root-Raised Cosine filters, for "
		"inputs between -1 and 1",
		"FiltersEtc", // categories
		"",
		"alpha(real): roll-off factor, between 0 and 1;\
											  lsbIn(int): position of the LSB of the input, e.g. -15 for a 16-bit signed input; \
											  lsbOut(int): position of the LSB of the output;\
                        n(int): filter order (number of taps will be 2n+1)",
		""};
}


