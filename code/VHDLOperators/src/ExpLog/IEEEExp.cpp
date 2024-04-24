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
#include "flopoco/ExpLog/IEEEExp.hpp"
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

#define LARGE_PREC 1000 // 1000 bits should be enough for everybody

namespace flopoco{


	IEEEExp::IEEEExp(
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
		name << "IEEEExp_" << wE << "_" << wF ;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("F. de Dinechin, Bogdan Pasca (2008-2021)");
		srcFileName="IEEEExp";

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

		addIEEEInput("X", wE, wFIn);
		addIEEEOutput("R", wE, wF);


		addConstant("wE", "positive", wE);
		addConstant("wF", "positive", wF);
		addConstant("wFIn", "positive", wFIn);
		addConstant("g", "positive", g);

		int bias = (1<<(wE-1))-1;
		if(bias < wF+g){
			ostringstream e;
			e << "ERROR in IEEEExp, unable to build architecture if wF+g > 2^(wE-1)-1." <<endl;
			e << "      Try increasing wE." << endl;
			e << "      If you really need IEEEExp to work with such values, please report this as a bug :)" << endl;
			throw e.str();
		}

		//******** Input unpacking and shifting to fixed-point ********
		// Unpack X
		int w = wE + wF +1;
		// sign
		vhdl << tab << declare(.0, "X_s", 1, false) << "<= X" << of(w-1) << ";" << endl;
		// exp
		vhdl << tab << declare(.0, "X_e_tmp", wE) << "<= " << "X" << range(w - 2, wF) << ";" << endl;
		// normal
		vhdl << tab << declare(target->adderDelay(wE), "X_exp_is_not_zero", 1, false) << "<= '0' when X_e_tmp=\"" << string(wE, '0') << "\" else '1';" << endl;
		// mantissa
		vhdl << tab << declare(.0, "X_f", wF+1) << "<= X_exp_is_not_zero & X"  << range(wF -1, 0) << ";" << endl;
		// Is mantissa empty
		vhdl << tab << declare(target->adderDelay(wF), "X_empty_m", 1, false) << "<= '1' when X"  << range(wF -1, 0) << " = \""  << string(wF, '0') << "\" else '0';" << endl;
		// detect nan and inf
		vhdl << tab << declare(target->adderDelay(wE), "X_nan_or_inf", 1, false) << "<= '1' when X_e_tmp = \"" << string(wE, '1') << "\" else '0';" << endl;
		vhdl << tab << declare("X_nan", 1, false) << " <= X_nan_or_inf AND (NOT X_empty_m);" << endl;
		vhdl << tab << declare("X_sig_nan", 1,  false) << " <= X_nan_or_inf AND (NOT X_empty_m) AND (NOT X" << of(wF-1) << ");" << endl;
		vhdl << tab << declare("X_inf", 1, false) << " <= X_nan_or_inf AND X_empty_m;" << endl;
		vhdl << tab << declare("X_zero", 1, false) << " <= NOT(X_exp_is_not_zero) AND X_empty_m;" << endl;  
		
		vhdl << tab << declare("X_normal", 1, false) << " <= X_exp_is_not_zero OR X_empty_m;" << endl;

		vhdl << declare("X_e", wE) << " <= X_e_tmp when X_exp_is_not_zero=\'1\' else  \"" << string(wE-1, '0') << "1\";" << endl;

		vhdl << tab  << declare("XSign") << " <= X_s;" << endl;
		vhdl << tab  << declare("XexpField", wE) << " <= X_e;" << endl;
		

		int e0 = bias - (wF+g);
		vhdl << tab  << declare("e0", wE+2) << " <= conv_std_logic_vector(" << e0 << ", wE+2);  -- bias - (wF+g)" << endl;
		vhdl << tab  << declare(getTarget()->adderDelay(wE+2), "shiftVal", wE+2) << " <= (\"00\" & XexpField) - e0; -- for a left shift" << endl;

		vhdl << tab  << "-- underflow when input is shifted to zero (shiftval<0), in which case exp = 1" << endl;
		vhdl << tab  << declare("resultWillBeOne") << " <= shiftVal(wE+1);" << endl;

		// As we don't have a signed shifter, shift first, complement next. TODO? replace with a signed shifter
		vhdl << tab << "--  mantissa with implicit bit" << endl;
		vhdl << tab  << declareFixPoint("mXu", false, 0,-wFIn) << " <= unsigned(X_f);" << endl;

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
		inmap << "XSign=>XSign";

		outmap << "expY=>expY,";
		outmap << "K=>K";


		cout << param.str() <<endl;

		newInstance("Exp", "exp_helper", param.str(), inmap.str(), outmap.str());

		// The following is generic normalization/rounding code if we have in expY an approx of exp(y) of size 	sizeExpY
		// with MSB of weight 2^1

		vhdl << tab << "-- Rounding" << endl;

		// might need to recode this to make it work
		// K is signed
		// TODO :
		// FPNumbers have normal numbers with exponent 0, not IEEE
		// number is subnormal when K + bias >= - wF
		// shift number is - (K + bias -1) if need no norm, otherwise -(K + bias)

		vhdl << tab << declare("needNoNorm") << " <= expY(" << sizeExpY-1 << ");" << endl;
		vhdl << tab << declare("expY_norm", sizeExpY) << "<= expY" << range(sizeExpY-1, 0) << " when needNoNorm='1' else expY" << range(sizeExpY-2, 0) << "& \"0\";" << endl;
		vhdl << tab << declare("K_extended", wE+2) << "<= K(" << wE << ") & K;" << endl;
		// If needNoNorm=0 then  E = K + bias -1 else E = K+bias
		// add +1 because we need to shift if E=0
		vhdl << tab << declare("preshiftSubnormalAmountMin1", wE+2) << "<= - ( signed(K_extended) + signed(conv_std_logic_vector(" << bias << ","<< wE+2 << ")) - 1 -1);" << endl;
		vhdl << tab << declare("preshiftSubnormalAmountZero", wE+2) << "<= - ( signed(K_extended) + signed(conv_std_logic_vector(" << bias << ","<< wE+2 << "))-1);" << endl;
		vhdl << tab << declare("preshiftSubnormalAmount", wE+2) << "<= preshiftSubnormalAmountMin1 when needNoNorm='0' else preshiftSubnormalAmountZero;" << endl;
		
		// isn't a subnormal if we don't have to shift
		vhdl << tab << declare("notSubnormal") << " <= '1' when preshiftSubnormalAmount" << of(wE+1) << "='1' or preshiftSubnormalAmount=\"" << string(wE+2, '0') << "\"  else '0';" << endl;
		
		int shiftsize = intlog2(sizeExpY);
	
		addComment("Flush to zero command only works when detected as subnormal (underflow) ");
		vhdl << tab << declare("flushed_to_zero") << " <= \'0\' when preshiftSubnormalAmount" << range(wE+1, shiftsize) << " = \"" << string(wE+1-shiftsize+1, '0') << "\" else \'1\';" << endl; 

		// real shift : shift 0 if not subnormal
		// else shift max if too big
		// else shift normal value
		vhdl << tab << declare("shiftSubnormalAmount", shiftsize) << "<= conv_std_logic_vector(0, "<< shiftsize << ") when notSubnormal='1' else" << endl
		<< tab << tab << "conv_std_logic_vector(" << sizeExpY << ", " << shiftsize << ") when flushed_to_zero='1' else " << endl
		<< tab << tab << "preshiftSubnormalAmount" << range(shiftsize-1, 0) << ";" << endl;
		
			newInstance("Shifter",
						"subnormal_shift",
						"wX=" + to_string(sizeExpY) + " maxShift=" + to_string(sizeExpY) + " wR=" + to_string(sizeExpY)+ " dir=1",
						"X=>expY_norm,S=>shiftSubnormalAmount",
						"R=>prepreRoundBiasSig");

		vhdl << tab << declare(getTarget()->logicDelay(), "preRoundBiasSig", wE+wF+2)
		<< " <= " << rangeAssign(wE+1, 0, "'0'") << " & prepreRoundBiasSig" << range(sizeExpY-2, sizeExpY-2-wF+1) << " when notSubnormal = '0'" << endl
		<< tab << tab << " else conv_std_logic_vector(" << bias << ", wE+2)  & expY" << range(sizeExpY-2, sizeExpY-2-wF+1) << " when needNoNorm = '1'" << endl
		<< tab << tab << "else conv_std_logic_vector(" << bias-1 << ", wE+2)  & expY" << range(sizeExpY-3, sizeExpY-3-wF+1) << " ;" << endl;

		/*
		vhdl << tab << declare(getTarget()->logicDelay(), "preRoundBiasSig", wE+wF+2)
		<< " <= conv_std_logic_vector(" << bias << ", wE+2)  & expY" << range(sizeExpY-2, sizeExpY-2-wF+1) << " when needNoNorm = '1'" << endl
		<< tab << tab << "else conv_std_logic_vector(" << bias-1 << ", wE+2)  & expY" << range(sizeExpY-3, sizeExpY-3-wF+1) << " ;" << endl;
		*/

		vhdl << tab << declare("roundBit") << " <= prepreRoundBiasSig(" << sizeExpY-2-wF << ")  when notSubnormal = '0' else expY(" << sizeExpY-2-wF << ")  when needNoNorm = '1'    else expY(" <<  sizeExpY-3-wF << ") ;" << endl;
		vhdl << tab << declare("roundNormAddend", wE+wF+2) << " <= K(" << wE << ") & K & "<< rangeAssign(wF-1, 1, "'0'") << " & roundBit when notSubnormal='1' else " << rangeAssign(wE+2+wF-1, 1, "'0'") << " & roundBit;" << endl;


		newInstance("IntAdder",
								"roundedExpSigOperandAdder",
								"wIn=" + to_string(wE+wF+2),
								"X=>preRoundBiasSig,Y=>roundNormAddend",
								"R=>roundedExpSigRes",
								"Cin=>'0'");

		// TODO add shifter for subnormal at some point

		vhdl << tab << declare(getTarget()->logicDelay(), "roundedExpSig", wE+wF+2) << " <= roundedExpSigRes when X_nan_or_inf=\'0\' else "
		<< "\"" << string(wE+wF+2, '1') << "\"; -- number in the normal case" << endl;

		vhdl << tab << declare(getTarget()->logicDelay(), "ofl1") << " <= not XSign and overflow0 and X_normal; -- input positive, normal,  very large" << endl;
		vhdl << tab << declare("exp_is_max") << "<= '1' when (roundedExpSig" << range(wE+wF-1, wF) << "= \"" << string(wE, '1') << "\") else '0';" << endl;
		vhdl << tab << declare("ofl2") << " <= not XSign and ((roundedExpSig(wE+wF) and not roundedExpSig(wE+wF+1)) OR exp_is_max) and X_normal; -- input positive, normal, overflowed" << endl;
		vhdl << tab << declare("ofl3") << " <= not XSign and X_inf;  -- input was +infty" << endl;
		vhdl << tab << declare("ofl") << " <= ofl1 or ofl2 or ofl3;" << endl;

		vhdl << tab << declare("ufl1") << " <= flushed_to_zero and not notSubnormal and X_normal; -- input normal" << endl;
		vhdl << tab << declare("ufl2") << " <= XSign and X_inf;  -- input was -infty" << endl;
		vhdl << tab << declare("ufl3") << " <= XSign and overflow0  and X_normal; -- input negative, normal,  very large" << endl;

		vhdl << tab << declare("ufl") << " <= ufl1 or ufl2 or ufl3;" << endl;

		vhdl << tab << "R <= \"0" << string(wE, '1') << "1" << string(wF-1, '0') << "\" when X_nan='1' else --nan" << endl
		<< tab << tab << " \"0" << string(wE, '1') << string(wF, '0') << "\" when ofl='1' else -- +infty" << endl
		<< tab << tab << " \"" << string(wE+wF+1, '0') << "\" when ufl='1' else -- 0" << endl
		<< tab << tab << " '0' & roundedExpSig" << range(wE+wF-1, 0) << ";" << endl;

		//flags
		// inexact flag makes no sense always 1 except if input is 0
		// overflow does 
		// underflow also (underflow if output is subnormal)
		// invalid does 
		// should exp(-inf) = 0 ? yes

	}

	IEEEExp::~IEEEExp()
	{
	}



	void IEEEExp::emulate(TestCase * tc)
	{
		
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		IEEENumber fpx(wE, wF, svX);

		mpfr_t x, ru,rd;
		mpfr_init2(x,  1+wF);
		mpfr_init2(ru, 1+wF);
		mpfr_init2(rd, 1+wF);
		fpx.getMPFR(x);
		int ternary_round_rd = mpfr_exp(rd, x, GMP_RNDD);
		int ternary_round_ru = mpfr_exp(ru, x, GMP_RNDU);
		IEEENumber  fprd(wE, wF, rd, ternary_round_rd, MPFR_RNDD);
		IEEENumber  fpru(wE, wF, ru, ternary_round_ru, MPFR_RNDU);
		mpz_class svRD = fprd.getSignalValue();
		mpz_class svRU = fpru.getSignalValue();
		if (svRD == svRU && svRU ==1) // weird bug with subnormals, where RD doesn't flush to 0 properly
			svRD = 0;
		tc->addExpectedOutput("R", svRD);
		tc->addExpectedOutput("R", svRU);
		mpfr_clears(x, ru, rd, NULL);
	}



	void IEEEExp::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		mpfr_t x, y;
		IEEENumber *fx, *fy;
		// double d;

		mpfr_init2(x, 1+wF);
		mpfr_init2(y, 1+wF);

		tc = new TestCase(this);
		tc->addIEEEInput("X", log(2));
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 0.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", -0.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 2.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", 1.5);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", -1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", -2.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addIEEEInput("X", -3.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addComment("The largest number whose exp is finite");
		fx = new IEEENumber(wE, wF, IEEENumber::greatestNormal);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDN);
		//		cout << "A " << fx->getSignalValue() << endl;
		//		 d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;
		fy = new IEEENumber(wE, wF, y);
		tc->addIEEEInput("X", *fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number whose exp is infinite");
		mpfr_nextabove(y);
		fy = new IEEENumber(wE, wF, y);
		tc->addIEEEInput("X", *fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);


		tc = new TestCase(this);
		tc->addComment("The last number whose exp is nonzero");
		fx = new IEEENumber(wE, wF, IEEENumber::smallestSubNormal);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDU);

		// cout << "A " << fx->getSignalValue() << endl;
		// d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;

		fy = new IEEENumber(wE, wF, y);
		tc->addIEEEInput("X", *fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number whose exp flushes to zero");
		mpfr_nextbelow(y);
		fy = new IEEENumber(wE, wF, y);
		tc->addIEEEInput("X", *fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);


		tc = new TestCase(this);
		tc->addComment("The last number whose exp is nonzero, normal version");
		fx = new IEEENumber(wE, wF, IEEENumber::smallestNormal);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDU);

		// cout << "A " << fx->getSignalValue() << endl;
		// d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;

		fy = new IEEENumber(wE, wF, y);
		tc->addIEEEInput("X", *fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number who outputs a subnormal");
		mpfr_nextbelow(y);
		fy = new IEEENumber(wE, wF, y);
		tc->addIEEEInput("X", *fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);

		mpfr_clears(x, y, NULL);
	}





	// One test out of 8 fully random (tests NaNs etc)
	// All the remaining ones test numbers with exponents between -wF-3 and wE-2,
	// For numbers outside this range, exp over/underflows or flushes to 1.

	TestCase* IEEEExp::buildRandomTestCase(int i){
		TestCase *tc;
		tc = new TestCase(this);
		mpz_class x;
		mpz_class bias = ((1<<(wE-1))-1);
		/* Fill inputs */
		if ((i & 7) == 0) { //fully random
			x = getLargeRandom(wE+wF+1);
		}
		else
		{
				mpz_class e = (getLargeRandom(wE+wF) % (wE+wF+2) ) -wF-3; // Should be between -wF-3 and wE-2
				//cout << e << endl;
				e = bias + e;
				mpz_class sign = getLargeRandom(1);
				x  = getLargeRandom(wF) + (e << wF) + (sign<<(wE+wF));
			}
			tc->addInput("X", x);
		/* Get correct outputs */
			emulate(tc);
			return tc;
		}




		OperatorPtr IEEEExp::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
			int wE, wF, k, d, g;
			bool fullInput;
			ui.parseStrictlyPositiveInt(args, "wE", &wE);
			ui.parseStrictlyPositiveInt(args, "wF", &wF);
			ui.parsePositiveInt(args, "k", &k);
			ui.parsePositiveInt(args, "d", &d);
			ui.parseInt(args, "g", &g);
			ui.parseBoolean(args, "fullInput", &fullInput);
			return new IEEEExp(parentOp, target, wE, wF, k, d, g, fullInput);
		}



		TestList IEEEExp::unitTest(int testLevel)
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
	const OperatorDescription<IEEEExp> op_descriptor<IEEEExp> {
	    "IEEEExp", // name
	    "A faithful IEEE floating-point exponential function.",
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





