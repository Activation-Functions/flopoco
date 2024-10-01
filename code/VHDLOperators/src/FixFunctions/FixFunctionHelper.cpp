/*
  FixFunction object for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Aric team at Ecole Normale Superieure de Lyon
	then by the Socrate team at INSA de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

*/

#include <sstream>

#include "flopoco/FixFunctions/FixFunctionHelper.hpp"

namespace flopoco{
void emulate_fixfunction(FixFunction const & fixfunc, TestCase * tc, bool correctlyRounded) {
	mpz_class x = tc->getInputValue("X");
	mpz_class rNorD,ru;
	fixfunc.eval(x,rNorD,ru,correctlyRounded);
	//cerr << " x=" << x << " -> " << rNorD << " " << ru << endl; // for debugging
	tc->addExpectedOutput("Y", rNorD);
	if(!correctlyRounded)
		tc->addExpectedOutput("Y", ru);
}


TestList generateFixFunctionUnitTest(int testLevel, int methodSize) {
	// Method size : for which input size the method makes sense to use
	// methodSize = 0 : Table
	// methodSize = 1 : MultipartiteTable and SimplePoly
	// methodSize = 2 : PiecewisePoly

	// the static list of mandatory tests
	TestList testStateList;
	vector<pair<string,string>> paramList;

	// List of functions
	vector<string> function;
	vector<bool> signedIn;
	vector<int> msbOut;
	vector<bool> tableCompression;
	vector<bool> scaleOutput; // multiply output by (1-2^lsbOut) to prevent it  reaching 2^(msbOut+1) due to faithful rounding

	// List of parameters for the size of the input
	vector<int> input_size;
	vector<int> degree;

	// Same functions for all the test levels
	
	function.push_back("log(x/2+1)"); // f positive, increasing, concave
	signedIn.push_back(false);
	msbOut.push_back(-1);
	scaleOutput.push_back(false);
	tableCompression.push_back(true);

	function.push_back("1/(x+1)"); // f positive, decreasing, convex
	signedIn.push_back(false); 
	msbOut.push_back(0);
	scaleOutput.push_back(false); // input in [0,1) output in [0.5,1] but we don't want scaleOutput
	tableCompression.push_back(true);

	function.push_back("acos(x*0.8)-pi/2"); // f negative, decreasing, concave
	signedIn.push_back(false);
	msbOut.push_back(-1);
	scaleOutput.push_back(false);
	tableCompression.push_back(true);

	function.push_back("sinh(x)-1"); // f negative, increasing, convex
	signedIn.push_back(false);
	msbOut.push_back(-1);
	scaleOutput.push_back(true);
	tableCompression.push_back(true);


	if(testLevel >= TestLevel::EXHAUSTIVE) 
	{ // The exhaustive unit tests with extra functions

		// This is a regression test that used to trigger a compression bug
		function.push_back("(2/(2-x)-1)"); // f positive, increasing, convex		
		signedIn.push_back(false);
		msbOut.push_back(0);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		
		function.push_back("2^x-1");	// f positive, increasing, convex
		signedIn.push_back(false); 
		msbOut.push_back(0);
		scaleOutput.push_back(true); // input in [0,1) output in [0, 1) : need scaleOutput
		tableCompression.push_back(true);

		function.push_back("sin(pi/4*x)"); // f positive, increasing, concave
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		function.push_back("sin(pi/2*x)"); // f positive, increasing, concave but f'(1)=0
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(true);
		tableCompression.push_back(true);
	
		// It seems we don't manage yet the case when the function may get negative
		// but the code has been elegantly fixed: it now THROWERRORs
		function.push_back("log(0.5+x)");  // f negative then positive, increasing, concave
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(true);
		tableCompression.push_back(true);

		function.push_back("sin(x)"); // f positive, increasing, concave
		signedIn.push_back(true);
		msbOut.push_back(0);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		function.push_back("exp(x)");
		signedIn.push_back(true);
		msbOut.push_back(2);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		function.push_back("log(x+1)"); // f positive, increasing, concave
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		function.push_back("tanh(4*x)"); // f negative then positive, increasing, convex then concave
		signedIn.push_back(true);
		msbOut.push_back(-1);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		// Adding more functions

		function.push_back("cosh(x-1.7)+1"); // f positive, decreasing, convex with output in (2;4)
		signedIn.push_back(false);
		msbOut.push_back(2);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		function.push_back("cos(pi/2*(x+1))"); // f negative, decreasing, convex
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(true);
		tableCompression.push_back(true);

		function.push_back("log(x+0.7)-0.6"); // f negative, increasing, concave
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		function.push_back("cos(x*pi/2)"); // f positive decreasing concave
		signedIn.push_back(false);
		msbOut.push_back(-1);
		scaleOutput.push_back(true);
		tableCompression.push_back(true);

		// this is the sigmoid
		function.push_back("1/(1+exp(-4*x))"); // f positive increasing convex then concave
		signedIn.push_back(true);
		msbOut.push_back(-1);
		scaleOutput.push_back(false);
		tableCompression.push_back(true);

		// Figure out which size to test all these functions on
		if (methodSize == 0) {  // PlainTable
			input_size.push_back(-8);
			degree.push_back(1);
			input_size.push_back(-10);
			degree.push_back(1);
		}
		
		// Everything
		input_size.push_back(-12);
		degree.push_back(1);

		if (methodSize > 0) {  // Everything except PlainTable
			input_size.push_back(-14);
			degree.push_back(2);
			input_size.push_back(-24);
			degree.push_back(3);
		}

		if (methodSize==2) { // Only PiecewisePoly
			input_size.push_back(-30);
			degree.push_back(5);
			input_size.push_back(-32);
			degree.push_back(5);
		}
	}
	else
	{
		// Parameter size for Quick and Substantial tests
		if (methodSize == 0) { // PlainTable
			input_size.push_back(-8);
			degree.push_back(1);
		} else if (methodSize == 1) { // MultipartiteTable and SimplePoly
			input_size.push_back(-10);
			degree.push_back(1);
		} else if (methodSize == 2) { // PiecewisePoly
			input_size.push_back(-14);
			degree.push_back(2);
		}
	}

	for (size_t j= 0; j < input_size.size(); j++) {
		for (size_t i =0; i<function.size(); i++) {
			paramList.clear();
			string f = function[i];
			int lsbOut = input_size[j] + msbOut[i]; // to have inputSize=outputSize
			if (scaleOutput[i]) {
				f = "(1-1b"+to_string(lsbOut) + ")*(" + f + ")";
			}
			paramList.push_back(make_pair("f", f));
			paramList.push_back(make_pair("signedIn", to_string(signedIn[i]) ) );
			paramList.push_back(make_pair("lsbIn", to_string(input_size[j])));
			paramList.push_back(make_pair("lsbOut", to_string(lsbOut)));
			paramList.push_back(make_pair("plainVHDL","true"));
			paramList.push_back(make_pair("tableCompression", to_string(tableCompression[i])));
			paramList.push_back(make_pair("d", to_string(degree[j])));
			testStateList.push_back(paramList);
			}
		}


		REPORT(DETAIL, "Generated " << testStateList.size() << " test(s).");
		return testStateList;
}

} //namespace
