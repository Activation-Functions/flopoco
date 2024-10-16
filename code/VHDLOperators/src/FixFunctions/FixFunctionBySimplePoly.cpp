/*
  Polynomial Function Evaluator for FloPoCo
	This version uses a single polynomial for the full domain, so no table. To use typically after a range reduction.
  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2018.
  All rights reserved.

  */

/* 

Error analysis: see FixFunctionByPiecewisePoly.cpp


*/
#include <cmath>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include "flopoco/FixFunctions/FixFunctionBySimplePoly.hpp"
#include "flopoco/FixFunctions/FixFunctionEmulator.hpp"
#include "flopoco/FixFunctions/FixHornerEvaluator.hpp"
#include "flopoco/utils.hpp"

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0


	FixFunctionBySimplePoly::FixFunctionBySimplePoly(OperatorPtr parentOp, Target* target, string func, bool signedIn, int lsbIn, int lsbOut, bool finalRounding_):
		Operator(parentOp, target), finalRounding(finalRounding_){

		f = new FixFunction(func, signedIn, lsbIn, lsbOut);

		srcFileName="FixFunctionBySimplePoly";

		// You fix code by insulting the user he's using it wrongly
		if(finalRounding==false){
			THROWERROR("FinalRounding=false not implemented yet" );
		}

		if(signedIn==false){
			THROWERROR("signedIn=false not implemented yet" );
		}

		ostringstream name;
		name<<"FixFunctionBySimplePoly";
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin (2014-2020)");
		addHeaderComment("-- Evaluator for " +  f-> getDescription() + "\n");
		REPORT(LogLevel::VERBOSE, "Entering constructor, FixFunction description: " << f-> getDescription());

		addInput("X"  , -lsbIn + (signedIn?1:0));
		int outputSize = f->msbOut-lsbOut+1;
		addOutput("Y" ,outputSize);
		useNumericStd();

		if(f->signedIn)
			vhdl << tab << declareFixPoint("Xs", true, 0, lsbIn) << " <= signed(X);" << endl;
		else
			vhdl << tab << declareFixPoint("Xs", true, 0, lsbIn) << " <= signed('0' & X);  -- sign extension of X" << endl;

		// Polynomial approximation
		double targetApproxError = exp2(lsbOut-2);
		poly = new BasicPolyApprox(f, targetApproxError, -1);
		double approxErrorBound = poly->getApproxErrorBound();

		REPORT(LogLevel::DETAIL, "Found polynomial of degree " << poly->getDegree());
		REPORT(LogLevel::DETAIL, "Overall error budget = " << exp2(lsbOut) << "  of which approximation error = " << approxErrorBound
					 << " hence rounding error budget = "<< exp2(lsbOut-1) - approxErrorBound);


		int degree = poly->getDegree();
		
		vhdl << tab << "-- With the following polynomial, approx error bound is " << approxErrorBound << " ("<< log2(approxErrorBound) << " bits)" << endl;

		// Adding the round bit to the degree-0 coeff
		poly->getCoeff(0)->addRoundBit(lsbOut-1);

#if 0
		// The following is probably dead code. It was a fix for cases
		int oldLSB0 = poly->getCoeff(0)->LSB;
		// where BasicPolyApprox found LSB=lsbOut, hence the round bit is outside of MSB[0]..LSB
		// In this case recompute the poly, it would give better approx error 
		if(oldLSB0 != poly->getCoeff(0)->LSB) {
			// deliberately at info level, I want to see if it happens
			REPORT(LogLevel::DETAIL, "   addRoundBit has changed the LSB to " << poly->getCoeff(0)->LSB << ", recomputing the coefficients");
			for(int i=0; i<=degree; i++) {
				REPORT(LogLevel::DEBUG, poly->getCoeff(i)->report());
			}
			poly = new BasicPolyApprox(f->fS, degree, poly->getCoeff(0)->LSB, signedIn);
			poly->getCoeff(0)->addRoundBit(lsbOut-1);
		}
#endif

		for(int i=0; i<=degree; i++) {
			coeffMSB.push_back(poly->getCoeff(i)->MSB);
			coeffLSB.push_back(poly->getCoeff(i)->LSB);
			coeffSize.push_back(poly->getCoeff(i)->MSB - poly->getCoeff(i)->LSB +1);
		}

		REPORT(LogLevel::DETAIL, poly->report());

		for(int i=degree; i>=0; i--) {
			FixConstant* ai = poly->getCoeff(i);
			//REPORT(LogLevel::DETAIL, " C" << i << " = " << string(12) <<ai->getBitVector() << "  " << printMPFR(ai->fpValue)  );
			//vhdl << tab << "-- " << join("A",i) <<  ": " << ai->report() << endl;
			vhdl << tab << declareFixPoint(join("A",i), true, coeffMSB[i], coeffLSB[i])
					 << " <= " << ai->getBitVector(0 /*both quotes*/)
					 << ";  --" << ai->report();
			if(i==0)
				vhdl << "  ... includes the final round bit at weight " << lsbOut-1;
			vhdl << endl;
		}


		// In principle we should compute the rounding error budget and pass it to FixHornerEval
		// REPORT(LogLevel::DETAIL, "Now building the Horner evaluator for rounding error budget "<< roundingErrorBudget);
		
		
		// This is the same order as newwInstance() would do, but does not require to write a factory for this Operator
		schedule();
		inPortMap("Y", "Xs");
		for(int i=0; i<=degree; i++) {
			inPortMap(join("A",i), join("A",i));
		}
		outPortMap("R", "HornerOutput");
		vector<BasicPolyApprox*> pv; // because that's what FixHornerEvaluator expects
		pv.push_back(poly);
		OperatorPtr h = new  FixHornerEvaluator(this, target, 
																						lsbIn,
																						f->msbOut,
																						lsbOut,
																						pv
																						);
		vhdl << instance(h, "horner", false);
		
		vhdl << tab << "Y <= " << "std_logic_vector(HornerOutput);" << endl;
	}



	FixFunctionBySimplePoly::~FixFunctionBySimplePoly() {
		delete f;
	}



	void FixFunctionBySimplePoly::emulate(TestCase* tc){
		emulate_fixfunction(*f, tc);
	}

	void FixFunctionBySimplePoly::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;
		int lsbIn = f->lsbIn;
		bool signedIn = f->signedIn;
		// Testing the extremal cases
		tc = new TestCase(this);
		tc->addInput("X", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addInput("X", (mpz_class(1)<<(-lsbIn) ) -1);
		tc -> addComment("largest positive value, corresponding to 1");
		emulate(tc);
		tcl->add(tc);

		if(signedIn) {
			tc = new TestCase(this);
			tc->addInput("X", (mpz_class(1)<<(-lsbIn) ));
			tc -> addComment("Smallest two's complement value, corresponding to -1");
			emulate(tc);
			tcl->add(tc);
		}
	}





	TestList FixFunctionBySimplePoly::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
		vector<string> functionList;

		functionList.push_back("sin(x)");
		functionList.push_back("exp(x)");
		functionList.push_back("log(x/2+1)");
		
    if(testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
			for (size_t i=0; i<functionList.size(); i++) {
					// first deg 2 and 3, 15 bits, exhaustive test, then deg 5 for 25 bits 
					string f = functionList[i];
					paramList.push_back(make_pair("f",f));
					paramList.push_back(make_pair("plainVHDL","true"));
					paramList.push_back(make_pair("lsbOut","-15"));
					paramList.push_back(make_pair("lsbIn","-15"));
					paramList.push_back(make_pair("TestBench n=","-2"));
					testStateList.push_back(paramList);
					paramList.clear();
					
					paramList.push_back(make_pair("f",f));
					paramList.push_back(make_pair("plainVHDL","true"));
					paramList.push_back(make_pair("lsbOut","-24"));
					paramList.push_back(make_pair("lsbIn","-24"));
					testStateList.push_back(paramList);
					paramList.clear();

					paramList.push_back(make_pair("f",f));
					paramList.push_back(make_pair("plainVHDL","true"));
					paramList.push_back(make_pair("lsbOut","-30"));
					paramList.push_back(make_pair("lsbIn","-30"));
					testStateList.push_back(paramList);
					paramList.clear();

					paramList.push_back(make_pair("f",f));
					paramList.push_back(make_pair("plainVHDL","true"));
					paramList.push_back(make_pair("lsbOut","-32"));
					paramList.push_back(make_pair("lsbIn","-32"));
					testStateList.push_back(paramList);
					paramList.clear();
				}
	}
		else     
		{
				// finite number of random test computed out of testLevel
		}	

		return testStateList;
	}




	
	OperatorPtr FixFunctionBySimplePoly::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
	{
		string f;
		bool signedIn;
		int lsbIn, lsbOut;

		ui.parseString(args, "f", &f);
		ui.parseBoolean(args, "signedIn", &signedIn);
		ui.parseInt(args, "lsbIn", &lsbIn);
		ui.parseInt(args, "lsbOut", &lsbOut);

		return new FixFunctionBySimplePoly(parentOp, target, f, signedIn, lsbIn, lsbOut);
	}

	template <>
	const OperatorDescription<FixFunctionBySimplePoly> op_descriptor<FixFunctionBySimplePoly> {
	    "FixFunctionBySimplePoly",
	    "Evaluator of function f on [0,1) or [-1,1), using a single "
	    "polynomial with Horner scheme",
	    "FunctionApproximation",
	    "",
	    "f(string): function to be evaluated between double-quotes, for instance \"exp(x*x)\";\
			signedIn(bool)=true: if true the function input range is [-1,1), if false it is [0,1);\
			lsbIn(int): weight of input LSB, for instance -8 for an 8-bit input;\
			lsbOut(int): weight of output LSB;",
	    "This operator uses a table for coefficients, and Horner "
	    "evaluation with truncated multipliers sized just right.<br>For "
	    "more details, see <a "
	    "href=\"bib/flopoco.html#DinJolPas2010-poly\">this article</a>."};
}
