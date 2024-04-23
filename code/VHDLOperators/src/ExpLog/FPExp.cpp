/*
  An FP exponential for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/
#include <cmath> // For NaN
#include <fstream>
#include <iostream>
#include <sstream>

#include "flopoco/ConstMult/FixRealKCM.hpp"
#include "flopoco/ExpLog/FPExp.hpp"
#include "flopoco/FixFunctions/FixFunctionByPiecewisePoly.hpp"
#include "flopoco/FixFunctions/FixFunctionByTable.hpp"
#include "flopoco/IntAddSubCmp/IntAdder.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/ShiftersEtc/Shifters.hpp"
#include "flopoco/Tables/DiffCompressedTable.hpp"
#include "flopoco/Tables/TableOperator.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"
#include "flopoco/utils.hpp"

using namespace std;

/* TODOs
Obtaining 400MHz in Exp 8 23 depends on the version of ISE. Test with recent one.
remove the nextCycle after the multiplier

check the multiplier in the case 8 27: logic only, why?

Pass DSPThreshold to PolyEval

replace the truncated mult and following adder with an FixedMultAdd
Clean up poly eval and bitheapize it


All the tables could be FixFunctionByTable...
*/

#define LARGE_PREC 1000 // 1000 bits should be enough for everybody

namespace flopoco{


	FPExp::FPExp(
							 OperatorPtr parentOp, Target* target,
							 int wE_, int wF_,
							 int k_, int d_, int guardBits, bool fullInput
							 ):
		Operator(parentOp, target),
		wE(wE_),
		wF(wF_)
	{
		// Paperwork

		ostringstream name;
		name << "FPExp_" << wE << "_" << wF ;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("F. de Dinechin, Bogdan Pasca (2008-2021)");
		srcFileName="FPExp";

		int blockRAMSize = getTarget()->sizeOfMemoryBlock();

		ExpArchitecture* myExp = new ExpArchitecture(blockRAMSize, wE_, wF_, k_, d_, guardBits, fullInput);

		// Various architecture parameter to be determined before attempting to
		// build the architecture
		this->k = myExp->getk();
		this->d = myExp->getd();
		this->g = myExp->getg();

		bool expYTabulated = myExp->getExpYTabulated();
		bool useTableExpZm1 = myExp->getUseTableExpZm1();
		bool useTableExpZmZm1 = myExp->getUseTableExpZmZm1();
		int sizeY = myExp->getSizeY();
		int sizeZ = myExp->getSizeZ();
		int sizeExpY = myExp->getSizeExpY();
		int sizeExpA = myExp->getSizeExpA();
		int sizeZtrunc = myExp->getSizeZtrunc();
		int sizeExpZmZm1 = myExp->getSizeExpZmZm1();
		int sizeExpZm1 = myExp->getSizeExpZm1();
		int sizeMultIn = myExp->getSizeMultIn();
		int MSB = myExp->getMSB();
		int LSB = myExp->getLSB();

		// nY is in [-1/2, 1/2]

		int wFIn; // The actual size of the input
		if(fullInput)
			wFIn=wF+wE+g;
		else
			wFIn=wF;

		addFPInput("X", wE, wFIn);
		addFPOutput("R", wE, wF);


		addConstant("wE", "positive", wE);
		addConstant("wF", "positive", wF);
		addConstant("wFIn", "positive", wFIn);
		addConstant("g", "positive", g);

		int bias = (1<<(wE-1))-1;
		if(bias < wF+g){
			ostringstream e;
			e << "ERROR in FPExp, unable to build architecture if wF+g > 2^(wE-1)-1." <<endl;
			e << "      Try increasing wE." << endl;
			e << "      If you really need FPExp to work with such values, please report this as a bug :)" << endl;
			throw e.str();
		}



		//******** Input unpacking and shifting to fixed-point ********

		vhdl << tab  << declare("Xexn", 2) << " <= X(wE+wFIn+2 downto wE+wFIn+1);" << endl;
		vhdl << tab  << declare("XSign") << " <= X(wE+wFIn);" << endl;
		vhdl << tab  << declare("XexpField", wE) << " <= X(wE+wFIn-1 downto wFIn);" << endl;
		vhdl << tab  << declareFixPoint("Xfrac", false, -1,-wFIn) << " <= unsigned(X(wFIn-1 downto 0));" << endl;

		int e0 = bias - (wF+g);
		vhdl << tab  << declare("e0", wE+2) << " <= conv_std_logic_vector(" << e0 << ", wE+2);  -- bias - (wF+g)" << endl;
		vhdl << tab  << declare(getTarget()->adderDelay(wE+2), "shiftVal", wE+2) << " <= (\"00\" & XexpField) - e0; -- for a left shift" << endl;

		vhdl << tab  << "-- underflow when input is shifted to zero (shiftval<0), in which case exp = 1" << endl;
		vhdl << tab  << declare("resultWillBeOne") << " <= shiftVal(wE+1);" << endl;

		// As we don't have a signed shifter, shift first, complement next. TODO? replace with a signed shifter
		vhdl << tab << "--  mantissa with implicit bit" << endl;
		vhdl << tab  << declareFixPoint("mXu", false, 0,-wFIn) << " <= \"1\" & Xfrac;" << endl;

		// left shift
		vhdl << tab  << "-- Partial overflow detection" << endl;
		int maxshift = wE-2+ wF+g; // maxX < 2^(wE-1);
		vhdl << tab  << declare("maxShift", wE+1) << " <= conv_std_logic_vector(" << maxshift << ", wE+1);  -- wE-2 + wF+g" << endl;
		vhdl << tab  << declare(getTarget()->adderDelay(wE+1),"overflow0") << " <= not shiftVal(wE+1) when shiftVal(wE downto 0) > maxShift else '0';" << endl;

		int shiftInSize = intlog2(maxshift);
		vhdl << tab  << declare("shiftValIn", shiftInSize) << " <= shiftVal" << range(shiftInSize-1, 0) << ";" << endl;
		newInstance("Shifter",
								"mantissa_shift",
								"wX=" + to_string(wFIn+1) + " maxShift=" + to_string(maxshift) + " dir=0",
								"X=>mXu,S=>shiftValIn",
								"R=>fixX0");

		int sizeShiftOut=maxshift + wFIn+1;
		int sizeXfix = wE-2 +wF+g +1; // still unsigned; msb=wE-1; lsb = -wF-g

		vhdl << tab << declareFixPoint("ufixX", false, wE-2, -wF-g) << " <= " << " unsigned(fixX0" <<
			range(sizeShiftOut -1, sizeShiftOut- sizeXfix) <<
			") when resultWillBeOne='0' else " << zg(sizeXfix) <<  ";" << endl;


		ostringstream param, inmap, outmap;
		param << "wE=" << wE;
		param << " wF=" << wF;
		param << " d=" << d;
		param << " k=" << k;
		param << " g=" << g;

		inmap << "ufixX_i=>ufixX,";
		inmap << "resultWillBeOne=>resultWillBeOne,";
		inmap << "overflow0=>overflow0,";
		inmap << "XSign=>XSign,";
		inmap << "Xexn=>Xexn";

		outmap << "R=>R";
	
		cout << param.str() <<endl;

		newInstance("Exp", "exp_helper", param.str(), inmap.str(), outmap.str());


	}

	FPExp::~FPExp()
	{
	}



	void FPExp::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wE, wF, svX);

		mpfr_t x, ru,rd;
		mpfr_init2(x,  1+wF);
		mpfr_init2(ru, 1+wF);
		mpfr_init2(rd, 1+wF);
		fpx.getMPFR(x);
		mpfr_exp(rd, x, GMP_RNDD);
		mpfr_exp(ru, x, GMP_RNDU);
		FPNumber  fprd(wE, wF, rd);
		FPNumber  fpru(wE, wF, ru);
		mpz_class svRD = fprd.getSignalValue();
		mpz_class svRU = fpru.getSignalValue();
		tc->addExpectedOutput("R", svRD);
		tc->addExpectedOutput("R", svRU);
		mpfr_clears(x, ru, rd, NULL);
	}



	void FPExp::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		mpfr_t x, y;
		FPNumber *fx, *fy;
		// double d;

		mpfr_init2(x, 1+wF);
		mpfr_init2(y, 1+wF);



		tc = new TestCase(this);
		tc->addFPInput("X", log(2));
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", FPNumber::plusDirtyZero);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", FPNumber::minusDirtyZero);
		emulate(tc);
		tcl->add(tc);



		tc = new TestCase(this);
		tc->addFPInput("X", 1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", 2.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", 1.5);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -2.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -3.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addComment("The largest number whose exp is finite");
		fx = new FPNumber(wE, wF, FPNumber::largestPositive);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDN);
		//		cout << "A " << fx->getSignalValue() << endl;
		//		 d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;
		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number whose exp is infinite");
		mpfr_nextabove(y);
		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);




		tc = new TestCase(this);
		tc->addComment("The last number whose exp is nonzero");
		fx = new FPNumber(wE, wF, FPNumber::smallestPositive);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDU);

		// cout << "A " << fx->getSignalValue() << endl;
		// d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;

		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number whose exp flushes to zero");
		mpfr_nextbelow(y);
		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);

		mpfr_clears(x, y, NULL);
	}





	// One test out of 8 fully random (tests NaNs etc)
	// All the remaining ones test numbers with exponents between -wF-3 and wE-2,
	// For numbers outside this range, exp over/underflows or flushes to 1.

	TestCase* FPExp::buildRandomTestCase(int i){
		TestCase *tc;
		tc = new TestCase(this);
		mpz_class x;
		mpz_class normalExn = mpz_class(1)<<(wE+wF+1);
		mpz_class bias = ((1<<(wE-1))-1);
		/* Fill inputs */
		if ((i & 7) == 0) { //fully random
			x = getLargeRandom(wE+wF+3);
		}
		else
		{
				mpz_class e = (getLargeRandom(wE+wF) % (wE+wF+2) ) -wF-3; // Should be between -wF-3 and wE-2
				//cout << e << endl;
				e = bias + e;
				mpz_class sign = getLargeRandom(1);
				x  = getLargeRandom(wF) + (e << wF) + (sign<<(wE+wF)) + normalExn;
			}
			tc->addInput("X", x);
		/* Get correct outputs */
			emulate(tc);
			return tc;
		}




		OperatorPtr FPExp::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
			int wE, wF, k, d, g;
			bool fullInput;
			ui.parseStrictlyPositiveInt(args, "wE", &wE);
			ui.parseStrictlyPositiveInt(args, "wF", &wF);
			ui.parsePositiveInt(args, "k", &k);
			ui.parsePositiveInt(args, "d", &d);
			ui.parseInt(args, "g", &g);
			ui.parseBoolean(args, "fullInput", &fullInput);
			return new FPExp(parentOp, target, wE, wF, k, d, g, fullInput);
		}



		TestList FPExp::unitTest(int testLevel)
		{
		// the static list of mandatory tests
			TestList testStateList;
			vector<pair<string,string>> paramList;
            std::vector<std::array<int, 6>> paramValues;

            paramValues = { // testing the default value on the most standard cases
                {5,  10, 0, 0, -1, 0},
                {8,  23, 0, 0, -1, 0},
                {11, 52, 0, 0, -1, 0}
            };
            if (testLevel == TestLevel::QUICK) {
                // just test paramValues
            }

        if (testLevel >= TestLevel::SUBSTANTIAL) {
            // The substantial unit tests
            // First test with plainVHDL, then with cool multipliers
        for (int wF=5; wF<53; wF+=1) {
            // test various input widths
            int nbByteWE = 6+(wF/10);
            while(nbByteWE>wF){
                nbByteWE -= 2;
            }
            paramList.push_back(make_pair("wF",to_string(wF)));
            paramList.push_back(make_pair("wE",to_string(nbByteWE)));
            paramList.push_back(make_pair("plainVHDL","true"));
            testStateList.push_back(paramList);
            paramList.clear();
        }
        for (int wF=5; wF<53; wF+=1) {
            // test various input widths
            int nbByteWE = 6+(wF/10);
            while(nbByteWE>wF){
                nbByteWE -= 2;
            }
            paramList.push_back(make_pair("wF",to_string(wF)));
            paramList.push_back(make_pair("wE",to_string(nbByteWE)));
            testStateList.push_back(paramList);
            paramList.clear();
        }
        }
      else
      {
          // finite number of random test computed out of testLevel
      }
        for (auto const params: paramValues) {
             paramList.push_back(make_pair("wE", to_string(params[0])));
             paramList.push_back(make_pair("wF", to_string(params[1])));
             paramList.push_back(make_pair("d", to_string(params[2])));
             paramList.push_back(make_pair("k", to_string(params[3])));
             paramList.push_back(make_pair("g", to_string(params[4])));
             paramList.push_back(make_pair("fullInput", to_string(params[5])));
             testStateList.push_back(paramList);
             paramList.clear();
        }
  		return testStateList;
	}

	template <>
	const OperatorDescription<FPExp> op_descriptor<FPExp> {
	    "FPExp", // name
	    "A faithful floating-point exponential function.",
	    "ElementaryFunctions",
	    "", // seeAlso
	    "wE(int): exponent size in bits; \
         wF(int): mantissa size in bits;  \
         d(int)=0: degree of the polynomial. 0 choses a sensible default.; \
         k(int)=0: input size to the range reduction table, should be between 5 and 15. 0 choses a sensible default.;\
         g(int)=-1: number of guard bits;\
         fullInput(bool)=0: input a mantissa of wF+wE+g bits (useful mostly for FPPow)",
	    "Parameter d and k control the DSP/RamBlock tradeoff. In both "
	    "cases, a value of 0 choses a sensible default. Parameter g is "
	    "mostly for internal use.<br> For all the details, see <a "
	    "href=\"bib/flopoco.html#DinechinPasca2010-FPT\">this "
	    "article</a>."};
}





