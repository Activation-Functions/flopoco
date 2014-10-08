/*
  Polynomial Function Evaluator for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  
  2008-2014.
  All rights reserved.

  */

#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "../utils.hpp"


#include "FixFunctionBySimplePoly.hpp"
#include "IntMult/FixMultAdd.hpp"

#ifdef HAVE_SOLLYA

using namespace std;

namespace flopoco{

#define DEBUGVHDL 0


	FixFunctionBySimplePoly::FixFunctionBySimplePoly(Target* target, string func, int lsbIn, int msbOut, int lsbOut, bool finalRounding_, bool plainStupidVHDL, map<string, double> inputDelays):
		Operator(target, inputDelays), finalRounding(finalRounding_){
		f=new FixFunction(func, lsbIn, msbOut, lsbOut);
		
		srcFileName="FixFunctionBySimplePoly";
		
		ostringstream name;
		name<<"FixFunctionBySimplePoly_"<<getNewUId(); 
		setName(name.str()); 

		setCopyrightString("Florent de Dinechin (2014)");
		addHeaderComment("-- Evaluator for " +  f-> getDescription() + "\n"); 
		addInput("X"  , -lsbIn);
		int outputSize = msbOut-lsbOut+1;
		addOutput("Y" ,outputSize , 2);
		useNumericStd();

		vhdl << tab << declareFixPoint("Xs", true, 0, lsbIn) << " <= signed('0' & X);  -- sign extension of X" << endl; 

		// Polynomial approximation
		double targetApproxError = pow(2,lsbOut-1); 
		poly = new BasicPolyApprox(f, targetApproxError, -1);
		double approxErrorBound = poly->approxErrorBound;

		int degree = poly->degree;
		if(msbOut<poly->coeff[0]->MSB) {
			REPORT(INFO, "user-provided msbO smaller that the MSB of the constant coefficient, I am worried it won't work");
		}
		vhdl << tab << "-- With the following polynomial, approx error bound is " << approxErrorBound << " ("<< log2(approxErrorBound) << " bits)" << endl;

		// Adding the round bit to the degree-0 coeff
		REPORT(DEBUG, "   A0 before adding round bit: " <<  poly->coeff[0]->report());
		poly->coeff[0]->addRoundBit(lsbOut-1);
		REPORT(DEBUG, "   A0 after adding round bit: " <<  poly->coeff[0]->report());

		for(int i=0; i<=degree; i++) {
			coeffMSB.push_back(poly->coeff[i]->MSB);
			coeffLSB.push_back(poly->coeff[i]->LSB);
			coeffSize.push_back(poly->coeff[i]->MSB - poly->coeff[i]->LSB +1);
		}

		for(int i=degree; i>=0; i--) {
			FixConstant* ai = poly->coeff[i];
			//			REPORT(DEBUG, " a" << i << " = " << ai->getBitVector() << "  " << printMPFR(ai->fpValue)  );
			//vhdl << tab << "-- " << join("A",i) <<  ": " << ai->report() << endl;
			vhdl << tab << declareFixPoint(join("A",i), true, coeffMSB[i], coeffLSB[i])
					 << " <= " << ai->getBitVector(0 /*both quotes*/)
					 << ";  --" << ai->report();
			if(i==0) 
				vhdl << "  ... includes the final round bit at weight " << lsbOut-1;
			vhdl << endl;
		}


		// Here we assume all the coefficients already include the proper number of guard bits
		int sigmaMSB=coeffMSB[degree];
		int sigmaLSB=coeffLSB[degree];
		vhdl << tab << declareFixPoint(join("Sigma", degree), true, sigmaMSB, sigmaLSB) 
				 << " <= " << join("A", degree)  << ";" << endl;

		for(int i=degree-1; i>=0; i--) {

			int xTruncLSB = max(lsbIn, sigmaLSB-sigmaMSB);

			int pMSB=sigmaMSB+0 + 1;
			sigmaMSB = max(pMSB-1, coeffMSB[i]) +1; // +1 to absorb addition overflow
			sigmaLSB = coeffLSB[i];

			resizeFixPoint(join("XsTrunc", i), "Xs", 0, xTruncLSB);			

			if(plainStupidVHDL) {
				vhdl << tab << declareFixPoint(join("P", i), true, pMSB,  sigmaLSB  + xTruncLSB /*LSB*/) 
						 <<  " <= "<< join("XsTrunc", i) <<" * Sigma" << i+1 << ";" << endl;
				// However the bit of weight pMSB is a 0. We want to keep the bits from  pMSB-1
				resizeFixPoint(join("Ptrunc", i), join("P", i), sigmaMSB, sigmaLSB);
				resizeFixPoint(join("Aext", i), join("A", i), sigmaMSB, sigmaLSB);
				
				vhdl << tab << declareFixPoint(join("Sigma", i), true, sigmaMSB, sigmaLSB)   << " <= " << join("Aext", i) << " + " << join("Ptrunc", i) << ";" << endl;
			}

			else { // using FixMultAdd
				REPORT(LIST, " i=" << i);
				FixMultAdd::newComponentAndInstance(this,
																						join("Step",i),     // instance name
																						join("XsTrunc",i),  // x
																						join("Sigma", i+1), // y
																						join("A", i),       // a
																						join("Sigma", i),   // result 
																						true, sigmaMSB, sigmaLSB  // signed, outMSB, outLSB
																						);
			}
		}

		resizeFixPoint("Ys", "Sigma0",  msbOut, lsbOut);

		vhdl << tab << "Y <= " << "std_logic_vector(Ys);" << endl;
	}



	FixFunctionBySimplePoly::~FixFunctionBySimplePoly() {
		free(f);
	}
	


	void FixFunctionBySimplePoly::emulate(TestCase* tc){
		f->emulate(tc);
	}

	void FixFunctionBySimplePoly::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		tc = new TestCase(this); 
		tc->addInput("X", 0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this); 
		tc->addInput("X", (mpz_class(1) << f->wIn) -1);
		emulate(tc);
		tcl->add(tc);

	}

}
	
#endif //HAVE_SOLLYA	
