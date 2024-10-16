/*
   A divider by a floating-point constant for FloPoCo

  This file is part of the FloPoCo project developed by the Arenaire
  team at Ecole Normale Superieure de Lyon
  
  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2011.
  All rights reserved.
*/

 


#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "flopoco/ConstMult/FPConstDiv.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"
#include "flopoco/UserInterface.hpp"
#include "flopoco/utils.hpp"

using namespace std;


namespace flopoco{


	// The expert version 

	FPConstDiv::FPConstDiv(OperatorPtr parentOp, Target* target, int wEIn_, int wFIn_, int wEOut_, int wFOut_, vector<int> divisors_, int dExp_, int alpha_, int arch):
		Operator(parentOp, target), 
		wEIn(wEIn_), wFIn(wFIn_), wEOut(wEOut_), d(1), divisors(divisors_), wFOut(wFOut_), dExp(dExp_), alpha(alpha_)
	{
		for(auto factor: divisors){
			d *= factor;
		}
		if(wEOut==0)
			wEOut=wEIn;
		if(wFOut==0)
			wFOut=wFIn;
		srcFileName="FPConstDiv";
		ostringstream name;
		name <<"FPConstDiv_"<<wEIn<<"_"<<wFIn<<"_"<<wEOut<<"_"<<wFOut<<"_"<<d<< "_"  << arch<<"_";
		if(dExp>=0)
			name<<dExp<<"_";
		else
			name<<"M"<<-dExp<<"_";
		
		if(getTarget()->isPipelined()) 
			name << getTarget()->frequencyMHz() ;
		else
			name << "comb";
		uniqueName_ = name.str();
		
		uniqueName_=name.str();

		if(wEIn<3 || wEOut <3){
			ostringstream error;
			error << srcFileName << " (" << uniqueName_ << "): exponent size must be at least 3" <<endl;
			throw error.str();
		}

		if(d==0) {
			ostringstream o;
			o << srcFileName << " (" << uniqueName_ << "): ERROR in FPConstDiv, division by 0";
			throw o.str();
		}

			// Constant normalization
		while ((d % 2) ==0) {
				REPORT(LogLevel::DEBUG, "d is even, normalising");
				d = d >>1;
				dExp+=1;
		}
		mantissaIsOne = false;
		if(d==1) {
			REPORT(LogLevel::DETAIL, "Mantissa of d is 1, dividing by it will be easy"); 
			mantissaIsOne = true;
		}
		
		// Computing the binary64 value of the actual constant, taking exponent into account
		dd = d*intpow2(dExp);

		REPORT(LogLevel::DETAIL, "dividing by the constant " << dd);



		// Set up the IO signals
		addFPInput("X", wEIn, wFIn);
		addFPOutput("R", wEOut, wFOut);

		setCopyrightString("Florent de Dinechin (2007-2017)");

		int gamma = sizeInBits(d);
		int s = gamma-1;
		int h = d>>1; 
		int intDivSize = wFIn+1 + s+1;

		vhdl << tab << declare("x_exn",2) << " <=  X("<<wEIn<<"+"<<wFIn<<"+2 downto "<<wEIn<<"+"<<wFIn<<"+1);"<<endl;
		vhdl << tab << declare("x_sgn") << " <=  X("<<wEIn<<"+"<<wFIn<<");"<<endl;
		vhdl << tab << declare("x_exp", wEIn) << " <=  X("<<wEIn<<"+"<<wFIn<<"-1 downto "<<wFIn<<");"<<endl;
		vhdl << tab << declare("x_sig", wFIn+1) << " <= '1' & X("<<wFIn-1 <<" downto 0);"<<endl;

		vhdl << tab << declare(getTarget()->adderDelay(gamma+1), "Diffmd", gamma+1) << " <=  ('0' & x_sig" << range(wFIn, wFIn-gamma+1)<< ") - ('0' & CONV_STD_LOGIC_VECTOR(" << d << ", " << gamma <<")) ;" << endl;
		vhdl << tab << declare("mltd") << " <=   Diffmd("<< gamma<<");" << endl;


		if(d*intpow2(dExp) >1.0) { // only underflow possible
			vhdl <<endl << tab << "-- exponent processing. For this d we may only have underflow" << endl;
			
			vhdl << tab << declare(getTarget()->adderDelay(wEOut+1), "r_exp0", wEOut+1) << " <=  ('0' & x_exp) - ( CONV_STD_LOGIC_VECTOR(" << s+1+dExp << ", " << wEOut+1 <<")) + (not mltd);" << endl;
			
			vhdl << tab << declare("underflow") << " <=  r_exp0(" << wEOut << ");" << endl;
			vhdl << tab << declare("r_exp", wEOut) << " <=  r_exp0" << range(wEOut-1, 0) << ";" << endl;
			
			vhdl <<endl << tab << "-- exception flag processing"<<endl; 
			vhdl << tab << declare("r_exn", 2) << " <=  \"00\" when  x_exn=\"01\" and underflow='1' else x_exn" << ";" << endl;
		}
		else
			{
			vhdl <<endl << tab << "-- exponent processing. For this d we may only have overflow" << endl;
			
			vhdl << tab << declare(getTarget()->adderDelay(wEOut+1), "r_exp0", wEOut+1) << " <=  ('0' & x_exp) + ( CONV_STD_LOGIC_VECTOR(" << -(s+1+dExp) << ", " << wEOut+1 <<")) + (not mltd);" << endl;
			
			vhdl << tab << declare("overflow") << " <=  r_exp0(" << wEOut << ");" << endl;
			vhdl << tab << declare("r_exp", wEOut) << " <=  r_exp0" << range(wEOut-1, 0) << ";" << endl;
			
			vhdl <<endl << tab << "-- exception flag processing"<<endl; 
			vhdl << tab << declare("r_exn", 2) << " <=  \"10\" when  x_exn=\"01\" and overflow='1' else x_exn" << ";" << endl;
			}

		// Back to where we were after the computation of mldt
		vhdl <<endl << tab << "-- significand processing"<<endl;
		if(mantissaIsOne) {
			vhdl << tab << declare("r_frac", wFOut) << " <= x_sig;"<<endl;
		}
		else {// Actual division
			// mux = diffusion of the control signal + 1 LUT
			vhdl << tab << declare("divIn0", intDivSize) << " <= '0' & x_sig & CONV_STD_LOGIC_VECTOR(" << h << ", " << s <<");" << endl;
			vhdl << tab << declare("divIn1", intDivSize) << " <= x_sig & '0' & CONV_STD_LOGIC_VECTOR(" << h << ", " << s <<");" << endl;
			vhdl << tab << declare(getTarget()->lutDelay(), "divIn", intDivSize) << " <= divIn1 when mltd='1' else divIn0;" << endl;
			
#if 0
			icd = new IntConstDiv(parentOp,target, intDivSize, d, alpha, arch);
			
			inPortMap  (icd, "X", "divIn");
			outPortMap (icd, "Q","quotient");
			outPortMap (icd, "R","remainder");
			vhdl << instance(icd, "sig_div");
#else
			// I feel silly rebuilding a string for the divisor list
			string divisorsString="";
			string colon="";
			for(auto factor: divisors){
				divisorsString+=  colon + to_string(factor) ;
				colon=":";
			}
			newInstance("IntConstDiv",
									"intconstdiv",
									"wIn=" + to_string(intDivSize) + " d="+ divisorsString
									+ " arch="+ to_string(arch) + " alpha="+ to_string(alpha) ,
									"X=>divIn",
									"Q=>quotient, R=>remainder");

#endif
			
			
			vhdl << tab << declare("r_frac", wFOut) << " <= quotient" << range(wFOut-1, 0) << ";"<<endl;
			
		}
			vhdl << tab << "R <=  r_exn & x_sgn & r_exp & r_frac" << ";" << endl;
	}




	FPConstDiv::~FPConstDiv() {
		// TODO but who cares really
		//		free(icd); TODO but what's the syntax?
	}





	void FPConstDiv::emulate(TestCase *tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wEIn, wFIn);
		fpx = svX;
		mpfr_t x, mpd, r;
		mpfr_init2(x, 1+wFIn);
		mpfr_init2(r, 1+wFOut); 
		mpfr_init(mpd); // Should be enough for everybody 
		mpfr_set_d(mpd, dd, GMP_RNDN);
		fpx.getMPFR(x);
		mpfr_div(r, x, mpd, GMP_RNDN);		
		// Set outputs 
		FPNumber  fpr(wEOut, wFOut, r);
		mpz_class svRN = fpr.getSignalValue();
		tc->addExpectedOutput("R", svRN);
		// clean up
		mpfr_clears(x, r, mpd, NULL);
		}


	void FPConstDiv::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;
		mpz_class x;
	
		tc = new TestCase(this); 
		tc->addFPInput("X", 1.0);
		tc->addComment("1.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("X", 3.0);
		tc->addComment("3.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("X", 9.0);
		tc->addComment("9.0");
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addFPInput("X", 27.0);
		tc->addComment("27.0");
		emulate(tc);
		tcl->add(tc);
	}



	TestList  FPConstDiv::unitTest(int testLevel)	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		std::vector<std::array<int, 4>> paramValues; //  order is wE wF d arch

		paramValues = { 
			{5,10, 3, 0}, 
			{8,23, 3, 0}, 
			{11,52, 3, 0},   
			{5,10, 3, 3}, 
			{8,23, 3, 3}, 
			{11,52, 3, 3},   
		};
		if(testLevel == TestLevel::QUICK)    {
			// TODO
    }
    else if(testLevel == TestLevel::SUBSTANTIAL){
			// TODO
		}
    else if(testLevel >= TestLevel::EXHAUSTIVE)	{
			// TODO
		}
		
		for (auto params: paramValues) {
			paramList.push_back(make_pair("wE", to_string(params[0])));
			paramList.push_back(make_pair("wF", to_string(params[1])));
			paramList.push_back(make_pair("d", to_string(params[2])));
			paramList.push_back(make_pair("arch", to_string(params[3])));
			testStateList.push_back(paramList);
			paramList.clear();
		}
		return testStateList;
	}


	
	OperatorPtr FPConstDiv::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wE,wF, dExp, alpha, arch;
		vector<int> divisors;
		ui.parseStrictlyPositiveInt(args, "wE", &wE); 
		ui.parseStrictlyPositiveInt(args, "wF", &wF);
		ui.parseColonSeparatedIntList(args, "d", &divisors);
		ui.parseInt(args, "dExp", &dExp);
		ui.parsePositiveInt(args, "arch", &arch);
		ui.parseInt(args, "alpha", &alpha);
		return new FPConstDiv(parentOp, target, wE, wF,  wE,  wF, divisors,  dExp, alpha, arch);
	}

template<>
const OperatorDescription<FPConstDiv> op_descriptor<FPConstDiv> {"FPConstDiv", // name
											 "Correctly rounded floating-point divider by a small constant.",
											 "ConstMultDiv",
											 "", // seeAlso
											 "wE(int): exponent size in bits; \
                        wF(int): mantissa size in bits;  \
                        d(int): small integer to divide by;  \
                        dExp(int)=0: binary exponent of d (the operator will divide by d.2^dExp);  \
											  arch(int)=0: architecture used for the mantissa IntConstDiv -- 0 for linear-time, 1 for log-time, 2 for multiply-and-add by the reciprocal; \
                        alpha(int)=-1: Algorithm uses radix 2^alpha. -1 choses a sensible default.",
											 "Correct rounding to the nearest (if you want other rounding modes contact us). This operator is described in <a href=\"bib/flopoco.html#dedinechin:2012:ensl-00642145:1\">this article</a>."
				};
}

