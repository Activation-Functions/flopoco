/*

FP2Fix, converting FP values to fixpoint ones. Can be used to convert FP to integer, too.

 Fabrizio Ferrandi wrote the original version.
 Rebuilt almost from scratch by Florent de Dinechin.

 exhaustive tests (to add to the autotest at some point)
./flopoco FP2Fix we=4 wf=3  MSB=7 LSB=0 TestBench
./flopoco FP2Fix we=5 wf=5  MSB=12 LSB=0 TestBench

*/

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <list>
#include <locale>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <array>

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include "flopoco/Conversions/FP2Fix.hpp"
#include "flopoco/IntAddSubCmp/IntAdder.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/ShiftersEtc/Shifters.hpp"
#include "flopoco/utils.hpp"




using namespace std;

namespace flopoco{
	
#define DEBUGVHDL 0


	FP2Fix::FP2Fix(Operator* parentOp, Target* target, int _MSB, int _LSB, int _wE, int _wF, bool _trunc_p) :
		Operator(parentOp, target), wE(_wE), wF(_wF), MSB(_MSB), LSB(_LSB),  trunc_p(_trunc_p)
	{
		srcFileName = "FP2Fix";
				
		ostringstream name;
		int absMSB = MSB>=0?MSB:-MSB;
		int absLSB = LSB>=0?LSB:-LSB;
		name<<"FP2Fix_" << wE << "_" << wF << "_"
				<< (MSB<0?"M":"") << absMSB << "_" << (LSB<0?"M":"")  << absLSB
				<< "_" << (trunc_p==1?"T":"R");
		setNameWithFreqAndUID(name.str());
		
		setCopyrightString("Fabrizio Ferrandi, Florent de Dinechin (2012-2023)");

		// Sanity checks
		if ((MSB < LSB)){
			THROWERROR("Input constraint LSB <= MSB not met.");
      }
		int w = MSB - LSB + 1;
		int maxExp = 1<<(wE-1);
		int minExp = 1 - maxExp;
		int minPos = minExp-wF;
		int padBitsMSB=0;
		int bias =  maxExp - 1;
		if(MSB > maxExp){
			REPORT(LogLevel::MESSAGE, "For wE=" << wE << ", maxExp=" << maxExp << " < MSB="<<MSB<< ". Some output bits are useless.");
			padBitsMSB = MSB-maxExp;
		}
		int padBitsLSB=0;
		if(LSB < minPos){
			REPORT(LogLevel::MESSAGE, "For wE=" << wE << " and  wF=" << wF << ", minExp-wF=" << minPos << " > LSB="<<LSB<< ". Some output bits are useless.");
			padBitsLSB=minPos-LSB;
		}

		/* Set up the IO signals */
		addFPInput ("X", wE,wF);
		addOutput("R", w); 
		addOutput ("ov");

		/*	VHDL code	*/
		vhdl << tab << declare("exn",2) << " <= X" << range(wE+wF+2,wE+wF+1) << ";"<<endl;
		vhdl << tab << declare("sign") << " <= X" << of(wE+wF) << ";"<<endl;
		vhdl << tab << declare("exponentField",wE) << " <= X" << range(wE+wF-1,wF) << ";"<<endl;
		vhdl << tab << declare("zero") << " <= '1' when exn=\"00\" else '0';"<<endl;
		vhdl << tab << declare("infNaN") << " <= '1' when exn(1)='1' else '0';"<<endl;
	
		/* 

			 First question: shift then negate, or negate then shift ?
			 We choose "shift then negate" because this allows to merge the negation with the rounding addition, so one single carry propagation. 

			 Now for alignment discussions.

			 We shift right because we accept to lose bits on the right;  
			 drawing with wE=3, MSB=7, LSB=0 of the two extremal shift values
			 0xxxx         shift=0	(leave one bit for the sign bit) (shift<0: overflow)
			         xxxx	 shift=maxShift			(shift>maxShift: return 0)	 
			 |      |r		 r is the round bit
			MSB		 LSB

			shift=0 when E-bias=MSB-1
			=> shift = (MSB+bias-1)- E
			and maxShift = w-1

			However in this drawing there remains one problematic case: if X = -2^MSB we should allow before negation
			 1000					 because -2^MSB is representable 	
			 |      |
			MSB		 LSB
			
			One option is to detect this specific case. It costs one we+wf constant comparator:
			cheap (on FPGAs) and no time overhead since it is performed on the input. 
      Only the higher bit of the mantissa needs to be updated, the rest of the architecture produces the proper lower bits. Unfortunately this update adds logic on the critical path

			Another option is to allow for one more bit in the shift:
			 xxxx					 shift=0	 (shift<0: overflow)
			         xxxx	 shift=maxShift			(shift>maxShift: return 0)	 
			 |      |r		 r is the round bit
			MSB		 LSB
			now maxShift=w. Then the negation adds the sign bit, so we get a fix number
			xxxxxxxxxx
			 |      |r		 r is the round bit
			MSB		 LSB
      and we need to add a second-level of overflow logic. This adds to the latency. 
			The larger shift adds log2(w) muxes but there may also be a threshold effect that adds one more level.
			Altogether the first option seems more desirable.
		*/

		if(padBitsLSB==0 && padBitsMSB==0) { // general case
			int maxShift = w-1; // see drawing above
			int shiftSize = intlog2(maxShift);
			// shift=MSB-E-bias = (MSB-bias) - E
			// In this case we also know that shiftSize <=wE, so let's perform the subtraction on this size, plus 1 bit to detect if < 0
			vhdl << tab << declare(getTarget()->adderDelay(wE+1), "shiftVal", wE+1)
					 << " <= " << unsignedBinary(signedToBitVector(MSB+bias-1, wE+1), wE+1, true) << "- ('0'&exponentField); -- shift right" << endl;		
			vhdl << tab << declare("overflow1") << " <= shiftVal" << of(wE) << "; --sign bit"<<endl;
			vhdl << tab << declare("underflow") << " <= '1' when signed(shiftVal) > " << signedToBitVector(maxShift, wE+1) << " else '0'; "<<endl;

			// If 1+wF > w  we may as well truncate the mantissa now to w bits only.
			int wFT = 2+wF > w ? w-1: wF;
			vhdl << tab << declare("mantissa",wFT+1) << " <= (not (zero or underflow)) & X" << range(wF-1, wF-wFT)<<";"<<endl;
			// the bit that flags the annoying asymetrical negative endpoint
			vhdl << tab << declare("minusTwoToMSB") << " <= '1'  when X"<<range(wE+wF+2, wF-wFT) << "=\"011" << unsignedBinary(mpz_class(MSB+bias), wE, false) << zg(wFT,-2) << "\" else '0'; -- -2^MSB"<<endl;
			vhdl << tab << declare("shiftValShort", shiftSize)
					 << " <= " << unsignedBinary(maxShift, shiftSize, true) << " when (underflow or zero)='1' else shiftVal" << range(shiftSize-1, 0) << ";" << endl; 

			// We use the cheapest shifter variant here.
			// Adding the computeSticky option would allow for RN ties to even
			// but it costs  20% more  area and delay, just to get the tie situations right.
			// Note that RNE is defined when the _output_ is floating point, not the case here 
			newInstance("Shifter",
									"FXP_shifter",
									"wX=" + to_string(wFT+1) + " wR=" + to_string(w) + " maxShift=" + to_string(maxShift) + " dir=1",
									"X=>mantissa, S=>shiftValShort",
									"R=>unsignedFixVal");
			//cerr << "fixValSize=" << getSignalByName("fixVal")->width()<< endl;
			// Now we have to negate AND perform rounding.
			// one addition each, but they can be merged
			vhdl << tab << declare("xoredFixVal",w+1) << " <= "
					 << rangeAssign(w,0,"sign") << " xor ('0' & unsignedFixVal);"<<endl;
			//			vhdl << tab << declare("roundcst",w+1) << " <= " << zg(w-1) << " & (sign and not (underflow or zero))  & (not sign and  not (underflow or zero));"<<endl;
			vhdl << tab << declare("roundcst",w+1) << " <= " << zg(w-1) << " & (sign)  & (not sign);"<<endl;
			newInstance("IntAdder", "roundAdder", "wIn="+to_string(w+1), "X=>xoredFixVal,Y=>roundcst", "R=>signedFixVal", "Cin=>'0'");
#if 0
			vhdl << tab << "R <= " << zg(w) << "when underflow='1' " <<endl
					 << tab << tab << " else (minusTwoToMSB or signedFixVal" << of(w) << ") & signedFixVal" << range(w-1,1) << ";" << endl;
#else
			vhdl << tab << "R <= (minusTwoToMSB or signedFixVal" << of(w) << ") & signedFixVal" << range(w-1,1) << ";"  << endl;
#endif
				
			vhdl << tab << "ov <= (not zero) and (infNaN or overflow1) and (not minusTwoToMSB);" << endl;
		}
		else {
			THROWERROR("Degenerate case currently unsupported");
		}

	}


	FP2Fix::~FP2Fix() {
	}


   void FP2Fix::emulate(TestCase * tc)
   {
      /* Get I/O values */
      mpz_class svX = tc->getInputValue("X");
      FPNumber  fpX(wE, wF, svX);
      mpfr_t x;
      mpfr_init2(x, 1+wF);
      fpX.getMPFR(x);
      //std::cerr << "FP " << printMPFR(i, 100) << std::endl;
      mpz_class svO;
      
      mpfr_t cst;
      mpfr_init2(cst, 10000); //init to infinite prec
      mpfr_set_ui(cst, 1 , GMP_RNDN);
      mpfr_mul_2si(cst, cst, -LSB+1 , GMP_RNDN); // cst = 2^(-LSB+1)
      mpfr_mul(x, x, cst, GMP_RNDN); // exact

			/*     if(trunc_p)
				mpfr_get_z(svO.get_mpz_t(), x, GMP_RNDD); // truncation 
      else
			*/
			// don't use GMP_RN here since we want to round to nearest, ties to up
			mpfr_get_z(svO.get_mpz_t(), x, GMP_RNDZ); // truncation
			svO +=1; // round bit
			svO = svO>>1; // and discard the round bit

			// Do we have an overflow ?
			int w=MSB-LSB+1;
			mpz_class minval = -(mpz_class(1)  << (w-1));
			mpz_class maxval = (mpz_class(1)  << (w-1)) -1;
			mpz_class exn = fpX.getExceptionSignalValue();
			bool infNaN = exn>=2;
			bool zero = exn==0;
			bool overflow = zero ? false : ((svO <minval) || (svO > maxval) || infNaN);
			//      std::cerr << "FIX " << svO << std::endl;
      tc->addExpectedOutput("ov", overflow?1:0);
			if(overflow) // we don't care what value is output
				tc->addExpectedOutputInterval("R", minval, maxval, TestCase::signed_interval);
			else
				tc->addExpectedOutput("R", svO, true /*signed*/); 
				
      // clean-up
      mpfr_clears(x,cst, NULL);
  }

#if 0 // disabled for now
  TestCase* FP2Fix::buildRandomTestCase(int i)
  {
     TestCase *tc;
     mpz_class a;
     tc = new TestCase(this); 
     mpz_class e = (getLargeRandom(wE+wF) % (MSB+(isSigned?0:1))); // Should be between 0 and MSB+1/0
     mpz_class normalExn = mpz_class(1)<<(wE+wF+1);
     mpz_class bias = ((1<<(wE-1))-1);
     mpz_class sign = isSigned ? getLargeRandom(1) : 0;
     e = bias + e;
     a = getLargeRandom(wF) + (e << wF) + (sign << wF+wE) + normalExn;
     tc->addInput("I", a);
     /* Get correct outputs */
     emulate(tc);
     return tc;		
   }
#endif

   void FP2Fix::buildStandardTestCases(TestCaseList* tcl){
	   // please fill me with regression tests or corner case tests!
		TestCase *tc;

		tc = new TestCase(this);
		tc->addFPInput("X", 0.0);
		tc->addComment("0.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", 1.0);
		tc->addComment("1.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -1.0);
		tc->addComment("-1.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -2.0);
		tc->addComment("-2.0");
		emulate(tc);
		tcl->add(tc);
   }

	TestList  FP2Fix::unitTest(int testLevel)	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		std::vector<std::array<int, 5>> paramValues;

		paramValues = { //  order is wE wF MSB LSB trunc
			{5,10, 7,-8	 , 1}, // conversion of pseudo FP16 to fixpoint	 
			{5,10, 7,-8	 , 0}, // conversion of pseudo FP16 to fixpoint	 
			{5,10, 7, 0	 , 1}, // conversion of pseudo FP16 to int8	 
			{5,10, 7, 0	 , 0}, // conversion of pseudo FP16 to int8	 
			{5,10, 0,-15 , 1}, // conversion of pseudo FP16 to fixpoint	 
			{5,10, -1,-16, 0}, // conversion of pseudo FP32 to fixpoint	 
			{5,10, 15,0	 , 1}, // conversion of pseudo FP16 to integer	
			{5,10, 15,0	 , 0}, // conversion of pseudo FP32 to integer	
			{8,23, 7,-8	 , 1}, // conversion of pseudo FP32 to fixpoint	 
			{8,23, 7,-8	 , 0}, // conversion of pseudo FP32 to fixpoint		
			{8,23, 0,-15 , 1}, // conversion of pseudo FP32 to fixpoint	 
			{8,23, -1,-16, 0}, // conversion of pseudo FP32 to fixpoint		
			{8,23, 23, 0 , 1}, // conversion of pseudo FP32 to integer
			{8,23, 23, 0 , 0}, // conversion of pseudo FP32 to integer
		};
		if(testLevel == TestLevel::QUICK)    { // The quick tests
    }
    else if(testLevel == TestLevel::SUBSTANTIAL)
			{ // The substantial unit tests
			}
    else if(testLevel >= TestLevel::EXHAUSTIVE)
			{ // The substantial unit tests
			}
		
		for (auto params: paramValues) {
			paramList.push_back(make_pair("wE", to_string(params[0])));
			paramList.push_back(make_pair("wF", to_string(params[1])));
			paramList.push_back(make_pair("MSB", to_string(params[2])));
			paramList.push_back(make_pair("LSB", to_string(params[3])));
			paramList.push_back(make_pair("trunc", to_string(params[4])));
			testStateList.push_back(paramList);
			paramList.clear();
		}
		return testStateList;
	}

		
	OperatorPtr FP2Fix::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wE, wF, MSB, LSB;
		bool trunc;
		ui.parseStrictlyPositiveInt(args, "wE", &wE); 
		ui.parseStrictlyPositiveInt(args, "wF", &wF);
		ui.parseInt(args, "MSB", &MSB); 
		ui.parseInt(args, "LSB", &LSB); 
		ui.parseBoolean(args, "trunc", &trunc);
		return new FP2Fix(parentOp, target,  MSB, LSB, wE, wF, trunc);
	}

	template <>
	const OperatorDescription<FP2Fix> op_descriptor<FP2Fix> {
	    "FP2Fix", // name
	    "Conversion from FloPoCo floating-point to fixed-point.",
	    "Conversions",
	    "", // seeAlso
	    "wE(int): input exponent size in bits;\
                        wF(int): input mantissa size in bits;\
                        MSB(int): weight of the MSB of the output;\
                        LSB(int): weight of LSB of the output;\
                        trunc(bool)=true: true means truncated (cheaper), false means rounded",
	    ""};
}
