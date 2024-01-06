/*
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
		name<<"FP2Fix_" << wE << "_" << wF << (LSB<0?"M":"") << "_" << absLSB << "_" << (MSB<0?"M":"") << absMSB <<"_" << "_" << (trunc_p==1?"T":"NT");
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
		if(MSB > maxExp){
			REPORT(LogLevel::MESSAGE, "For wE=" << wE << ", maxExp=" << maxExp << " < MSB="<<MSB<< ". Some output bits are useless.");
			padBitsMSB = MSB-maxExp;
		}
		int padBitsLSB=0;
		if(LSB < minPos){
			REPORT(LogLevel::MESSAGE, "For wE=" << wE << " and  wF=" << wF << ", minExp-wF=" << minPos << " > LSB="<<LSB<< ". Some output bits are useless.");
			padBitsLSB=minPos-LSB;
		}
		
		/*      int wFO0;
						if(eMax+1 < MSB + 1)
						wFO0 = eMax + 1 - LSB;
						else
						wFO0 = MSB + 1 - LSB;
							
						if (( maxExpWE < MSB ) || ( minExpWE > LSB)){
						THROWERROR("The exponent is too small for full coverage. Try increasing the exponent.");
						}
		*/
		/* Set up the IO signals */

		addFPInput ("X", wE,wF);
		addOutput("R", true, w); // signed
		addOutput ("ov");

		/*	VHDL code description	*/
		vhdl << tab << declare("eA0",wE) << " <= X" << range(wE+wF-1,wF) << ";"<<endl;
		vhdl << tab << declare("fA0",wF+1) << " <= \"1\" & X" << range(wF-1, 0)<<";"<<endl;
    int bias =  maxExp - 1;

		vhdl << tab << declare(getTarget()->adderDelay(wE), "eA1", wE) << " <= eA0 - conv_std_logic_vector(" << bias << ", "<< wE<<");"<<endl;
			
#if 0      
      int wShiftIn = intlog2(w+2);
      if(wShiftIn < wE)
        vhdl << tab << declare("shiftedby", wShiftIn) <<  " <= eA1" << range(wShiftIn-1, 0)                 << " when eA1" << of(wE-1) << " = '0' else " << rangeAssign(wShiftIn-1,0,"'0'") << ";"<<endl;
      else
        vhdl << tab << declare("shiftedby", wShiftIn) <<  " <= " << rangeAssign(wShiftIn-wE,0,"'0'") << " & eA1 when eA1" << of(wE-1) << " = '0' else " << rangeAssign(wShiftIn-1,0,"'0'") << ";"<<endl;

      //FXP shifter
			newInstance("Shifter",
									"FXP_shifter",
									"wX=" + to_string(wF+1) + " maxShift=" + to_string(wFO0+2) + " dir=0",
									"X=>fA0,S=>shiftedby",
									"R=>fA1");
			
      if(trunc_p)
      {
         if(!isSigned)
         {
            vhdl << tab << declare("fA4",wFO) <<  "<= fA1" << range(wFO0+wF+LSB, wF+1+LSB)<< ";"<<endl;
         }
         else
         {
            vhdl << tab << declare("fA2",wFO) <<  "<= fA1" << range(wFO0+wF+LSB, wF+1+LSB)<< ";"<<endl;
            vhdl << tab << declare(getTarget()->adderDelay(wFO),  "fA4",wFO) <<  "<= fA2 when I" << of(wE+wF) <<" = '0' else -isSigned(fA2);" <<endl;
         }
      }
      else
      {
         vhdl << tab << declare("fA2a",wFO+1) <<  "<= '0' & fA1" << range(wFO0+wF+LSB, wF+1+LSB)<< ";"<<endl;
         if(!isSigned)
         {
            vhdl << tab << declare(getTarget()->logicDelay(), "notallzero") << " <= '0' when fA1" << range(wF+LSB-1, 0) << " = " << rangeAssign(wF+LSB-1, 0,"'0'") << " else '1';"<<endl;
            vhdl << tab << declare(getTarget()->logicDelay(), "round") << " <= fA1" << of(wF+LSB) << " and notallzero ;"<<endl;
         }
         else
         {
            vhdl << tab << declare(getTarget()->logicDelay(), "notallzero") << " <= '0' when fA1" << range(wF+LSB-1, 0) << " = " << rangeAssign(wF+LSB-1, 0,"'0'") << " else '1';"<<endl;
            vhdl << tab << declare("round") << " <= (fA1" << of(wF+LSB) << " and I" << of(wE+wF) << ") or (fA1" << of(wF+LSB) << " and notallzero and not I" << of(wE+wF) << ");"<<endl;
         }   
         vhdl << tab << declare("fA2b",wFO+1) <<  "<= '0' & " << rangeAssign(wFO-1,1,"'0'") << " & round;"<<endl;
				 newInstance("IntAdder", "fracAdder", "wIn="+to_string(wFO+1), "X=>fA2a,Y=>fA2b", "R=fA3>", "Cin=>'1'");

				 if(!isSigned)
         {
            vhdl << tab << declare("fA4",wFO) <<  "<= fA3" << range(wFO-1, 0)<< ";"<<endl;
         }
         else
         {
            vhdl << tab << declare(getTarget()->adderDelay(wFO+1), "fA3b",wFO+1) <<  "<= -isSigned(fA3);" <<endl;
            vhdl << tab << declare(getTarget()->logicDelay(),  "fA4",wFO) <<  "<= fA3" << range(wFO-1, 0) << " when I" << of(wE+wF) <<" = '0' else fA3b" << range(wFO-1, 0) << ";" <<endl;
         }
      }
      if (eMax > MSB)
      {
         vhdl << tab << declare("overFl0") << "<= '1' when I" << range(wE+wF-1,wF) << " > conv_std_logic_vector("<< eMax+MSB << "," << wE << ") else I" << of(wE+wF+2)<<";"<<endl;
      }
      else
      {
         vhdl << tab << declare("overFl0") << "<= I" << of(wE+wF+2)<<";"<<endl;
      }
      
      if(trunc_p)
      {
         if(!isSigned)
            vhdl << tab << declare("overFl1") << " <= fA1" << of(wFO0+wF+1+LSB) << ";"<<endl;
         else
         {
					 vhdl << tab << declare(getTarget()->logicDelay(), "notZeroTest") << " <= '1' when fA4 /= conv_std_logic_vector(0," << wFO <<")"<< " else '0';"<<endl;
					 vhdl << tab << declare(getTarget()->logicDelay(), "overFl1") << " <= (fA4" << of(wFO-1) << " xor I" << of(wE+wF) << ") and notZeroTest;"<<endl;
        }
      }
      else
      {
         vhdl << tab << declare("overFl1") << " <= fA3" << of(wFO) << ";"<<endl;
      }

      vhdl << tab << declare(getTarget()->logicDelay(), "eTest") << " <= (overFl0 or overFl1);" << endl;

      vhdl << tab << "O <= fA4 when eTest = '0' else" << endl;
      vhdl << tab << tab << "I" << of(wE+wF) << " & (" << wFO-2 << " downto 0 => not I" << of(wE+wF) << ");"<<endl;
#endif
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
      mpfr_mul_2si(cst, cst, -LSB , GMP_RNDN); // cst = 2^(-LSB)
      mpfr_mul(x, x, cst, GMP_RNDN); // exact

      if(trunc_p)
				mpfr_get_z(svO.get_mpz_t(), x, GMP_RNDD); // truncation 
      else
				mpfr_get_z(svO.get_mpz_t(), x, GMP_RNDN); // round to nearest

			// Do we have an overflow ?
			int w=MSB-LSB+1;
			mpz_class minval = -(mpz_class(1)  << (w-1));
			mpz_class maxval = (mpz_class(1)  << (w-1)) -1;
			bool overflow = (svO <minval) || (svO > maxval);
			//      std::cerr << "FIX " << svO << std::endl;
      tc->addExpectedOutput("ov", overflow?1:0);
			if(overflow) // we don't care what value is output
				tc->addExpectedOutputInterval("R", minval, maxval, TestCase::signed_interval);
			else
				tc->addExpectedOutput("R", svO);
				
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
			paramList.push_back(make_pair("MSB", to_string(params[3])));
			paramList.push_back(make_pair("LSB", to_string(params[4])));
			paramList.push_back(make_pair("trunc", to_string(params[2])));
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
