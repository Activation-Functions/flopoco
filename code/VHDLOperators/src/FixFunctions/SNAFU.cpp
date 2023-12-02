/*

  This file is part of the FloPoCo project
  initiated by the Aric team at Ecole Normale Superieure de Lyon
  and developed by the Socrate team at Institut National des Sciences Appliquées de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA,
  2008-2014.
  All rights reserved.

*/
#include <iostream>
#include <iomanip>

#include "flopoco/FixFunctions/FixFunctionByTable.hpp"
#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/FixFunctions/FixFunctionEmulator.hpp"
#include "flopoco/FixFunctions/SNAFU.hpp"
#include "flopoco/IntMult/FixMultAdd.hpp"

using namespace std;




#define LARGE_PREC 1000 // 1000 bits should be enough for everybody


namespace flopoco{

	struct ActivationFunctionData {
		string stdShortName;
		string fullDescription;
		string sollyaString;
		bool signedOutput;
		bool needsSlightRescale; // for output range efficiency of functions that touch 1 
		bool reluVariant; 		
	};

	static map<string, ActivationFunctionData> activationFunction = {
		// two annoyances below:
		// 1/ we are not allowed spaces because it would mess the argument parser. Silly
		// 2/ no if then else in sollya, so we emulate with the quasi-threshold function 1/(1+exp(-1b256*X))
		{"sigmoid",
		 {
			 "Sigmoid",
			 "Sigmoid",
			 "1/(1+exp(-X))", //textbook
			 false, // output unsigned
			 true, // output touches 1 and needs to be slightly rescaled
			 false // not a ReLU variant
		 }
		},    
		{"tanh",
		 {
			 "TanH",
			 "Hyperbolic Tangent",
			 "tanh(X)",  //textbook
			 true, // output signed
			 true, // output touches 1 and needs to be slightly rescaled
			 false // not a ReLU variant
		 }
		},
		{"relu",
		 {
			 "ReLU",
			 "Rectified Linear Unit",
			 "X/(1+exp(-1b256*X))", // Here we use a quasi-threshold function
			 false, // output unsigned
			 false, // output does not need rescaling -- TODO if win>wout it probably does
			 true // ReLU variant
		 }
		},	
		{"elu",
		 {
			 "ELU",
			 "Exponential Linear Unit",
			 "X/(1+exp(-1b256*X))+expm1(X)*(1-1/(1+exp(-1b256*X)))", // Here we use a quasi-threshold function
			 true, // output signed
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 }
		},
		{"silu",
		 {
			 "SiLU",
			 "Sigmoid Linear Unit",
			 "X/(1+exp(-X))",  // textbook
			 true, // output signed
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 },    				 
		}, 
		{"gelu",
		 {
			 "GeLU",
			 "Gaussian Error Linear Unit",
			 "X/2*(1+erf(X/sqrt(2)))", // textbook
			 true, // output signed
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 }
		}
	};
	

	SNAFU::SNAFU(OperatorPtr parentOp_, Target* target_, string fIn,
							 int wIn_, int wOut_, string methodIn, double inputScale_, int adhocCompression_):
		Operator(parentOp_, target_), wIn(wIn_), wOut(wOut_),
		inputScale(inputScale_), adhocCompression(adhocCompression_)
	{

		if(inputScale<=0){
			throw(string("inputScale should be strictly positive"));
		}
		OperatorPtr op;
		string fl = to_lowercase(fIn);
		string method = to_lowercase(methodIn);


		auto af = activationFunction[fl];
		string sollyaFunction = af.sollyaString;
		string functionName = af.stdShortName;
		string functionDescription = af.fullDescription;
		bool signedOutput = af.signedOutput;
		bool needsSlightRescale = af.needsSlightRescale; // for output range efficiency of functions that touch 1 
		bool reluVariant = af.reluVariant; 

		auto reluf = activationFunction["relu"];
		string sollyaReLU = reluf.sollyaString;

		//determine the LSB of the input and output
		int lsbIn = -(wIn-1); // -1 for the sign bit
		int lsbOut; // will be set below, it depends on signedness and if the function is a relu variant


		///////// Ad-hoc compression consists, in the ReLU variants, to subtract ReLU 
		if(1==adhocCompression && !af.reluVariant) { // the user asked to compress a function that is not ReLU-like 
		REPORT(LogLevel::MESSAGE, " ??????? You asked for the compression of " << functionDescription << " which is not a ReLU-like" << endl
					 << " ??????? I will try but I am doubtful about the result." << endl
					 << " Proceeeding nevertheless..." << lsbOut);
		} 

		if(-1==adhocCompression) { // means "automatic" and it is the default 
			adhocCompression = af.reluVariant; 
		} // otherwise respect it.


		//////////////   Function preprocessing, for a good hardware match
		
		// scale the function by replacing X with (inputScale*x)
		// string replacement of "x" is dangerous because it may appear in the name of functions, so use @
		string replaceString = "("+to_string(inputScale)+"*@)";
		size_t pos;
		while((pos=sollyaFunction.find("X")) != string::npos) {
				sollyaFunction.replace(pos, 1, replaceString);
		}
		// then restore X 
		while((pos=sollyaFunction.find("@")) != string::npos) {
				sollyaFunction.replace(pos, 1, "X");
		}

		// do the same for the relu
		while((pos=sollyaReLU.find("X")) != string::npos) {
				sollyaReLU.replace(pos, 1, replaceString);
		}
		while((pos=sollyaReLU.find("@")) != string::npos) {
				sollyaReLU.replace(pos, 1, "X");
		}
		


		// if necessary scale the output so the value 1 is never reached
		if(needsSlightRescale) {
			lsbOut = -wOut +(signedOutput?1:0);
			sollyaFunction = "(1-1b"+to_string(lsbOut)+")*("+sollyaFunction+")";
			sollyaReLU = "(1-1b"+to_string(lsbOut)+")*("+sollyaReLU+")";
		}
		else if (reluVariant) {
			lsbOut = -wOut + intlog2(inputScale);
		}
		else {
			throw(string("unable to set lsbOut"));
		}
		const bool signedIn=true;

		// Only then we may define the delta function
		if(adhocCompression) {  
			sollyaDeltaFunction =  "("+sollyaReLU+")-("+sollyaFunction+")";
		}

		
		if (method=="auto") { // means "please choose for me"
			// TODO a complex sequence of if then else once the experiments are done
			method = "plaintable"; 
		}

		REPORT(LogLevel::MESSAGE, "Function after pre-processing: " << functionDescription << " evaluated on [-1,1)" << endl
					 << " wIn=" << wIn << " translates to lsbIn=" << lsbIn << endl
					 << " wOut=" << wOut << " translates to lsbOut=" << lsbOut);
		REPORT(LogLevel::MESSAGE, "Method is " << method);
		if (adhocCompression){
			REPORT(LogLevel::MESSAGE, "To plot the function with its delta function, copy-paste the following lines in Sollya" << endl
						 << "  f = " << sollyaFunction << ";" << endl
						 << "  deltaf = " << sollyaDeltaFunction << ";" << endl
						 << "  relu = " << sollyaReLU << ";" << endl
						 << "  plot(deltaf, [-1;1]); " << endl
						 << "  plot(f,relu,-deltaf, [-1;1]); " );
		}
		else{
			REPORT(LogLevel::MESSAGE, "To plot the function being implemented, copy-paste the following two lines in Sollya" << endl
						 << "  f = " << sollyaFunction << ";" << endl
						 << "  plot(f, [-1;1]); " );
		}

		bool correctlyRounded=false; // default is faithful
	 
		f = new FixFunction(sollyaFunction, signedIn, lsbIn, lsbOut);
	
		ostringstream name;
		name << functionName << "_" << wIn <<"_" << wOut <<"_" << method;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("Florent de Dinechin (2023)");

		addInput ("X", wIn);
		addOutput ("Y", wOut);

		if(!adhocCompression) {
			if(method=="plaintable") {
				addComment("This function is correctly rounded");
				string paramString = "f="+sollyaFunction + join(" lsbIn=", lsbIn)+ join(" lsbOut=", lsbOut) + " signedIn=true";
				OperatorPtr op = newInstance("FixFunctionByTable", functionName+"_Table",
																		 paramString,
																		 "X=>X", "Y=>TableOut");
				correctlyRounded=true;
				vhdl << tab << "Y <= TableOut ;" << endl;
				// sanity check
				if(int w = op->getSignalByName("X")->width() != wIn)  {
					REPORT(LogLevel::MESSAGE, "Something went wrong, operator input size is  " << w
								 << " instead of the requested wIn=" << wIn << endl << "Attempting to proceed nevertheless.");
				}
				if(int w = op->getSignalByName("Y")->width() != wOut)  {
					REPORT(LogLevel::MESSAGE, "Something went wrong, operator output size is  " << w
								 << " instead of the requested wOut=" << wOut << endl << "Attempting to proceed nevertheless.");
				}
				
			}
			else {
				throw(string("Method: ")+method + " currently unsupported");
			}
		}
		else { ///////////////// Ad-hoc compresssion
			// we have already tested that before and printed out something
			// if (!reluVariant) {
			// 	throw(string("Function ")+ fIn + " is not a ReLU variant, it doesn't make sense to apply adhocCompression");
			// }
			if(wIn!=wOut) {
				throw(string("Too lazy so far to support wIn<>wOut in case of ad-hoc compression "));
			};
			vhdl << tab << declare("ReLU", wOut) << " <= " << zg(wOut) << " when X" << of(wIn-1) << "='1'   else '0' & X" << range(wIn-2,0) << ";" << endl;
			string paramString = "f="+sollyaDeltaFunction + join(" lsbIn=", lsbIn)+ join(" lsbOut=", lsbOut) + " signedIn=false";
			//				cerr << "************  " << paramString << endl;
			OperatorPtr op = newInstance("FixFunctionByTable", functionName+"_deltaTable",
																	 paramString,
																	 "X=>X", "Y=>TableOut");
			// if all went well TableOut should have fewer bits so need to be padded with zeroes
			// we do that the flopoco way, not the clever VHDL way
			int tableOutSize = getSignalByName("TableOut")->width();
			vhdl << tab << declare("F", wOut) << " <= ReLU - (" << zg(wOut-tableOutSize) << " & TableOut);" << endl;
			vhdl << tab << "Y <= F ;" << endl;
			
		}

	}

		
	void SNAFU::emulate(TestCase* tc){
		emulate_fixfunction(*f, tc, true /* correct rounding */);
	}
	
	OperatorPtr SNAFU::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wIn, wOut;
		double inputScale;
		string fIn, methodIn;
		int adhocCompression;
		ui.parseString(args, "f", &fIn);
		ui.parseInt(args, "wIn", &wIn);
		ui.parseInt(args, "wOut", &wOut);
		ui.parseString(args, "method", &methodIn);
		ui.parseFloat(args, "inputScale", &inputScale);
		ui.parseInt(args, "adhocCompression", &adhocCompression);
		return new SNAFU(parentOp, target, fIn, wIn, wOut, methodIn, inputScale, adhocCompression);

	}



	
		template <>
		const OperatorDescription<SNAFU> op_descriptor<SNAFU> {
	    "SNAFU", // name
				"Simple Neural Activation Function Unit, without reinventing the wheel",
				"FunctionApproximation(HIDDEN)",
				"Also see generic options",
				"f(string): function of interest, among \"Tanh\", \"Sigmoid\", \"ReLU\", \"GELU\", \"ELU\", \"SiLU\" (case-insensitive);"
				"wIn(int): number of bits of the input ;" 
				"wOut(int): number of bits of the output; "
				"inputScale(real)=8.0: the input scaling factor: the 2^wIn input values are mapped on the interval[-inputScale, inputScale) ; "
				"method(string)=auto: approximation method, among \"PlainTable\", \"MultiPartite\", \"Horner2\", \"Horner3\", \"PiecewiseHorner2\", \"PiecewiseHorner3\", \"auto\" ;"
				"adhocCompression(int)=-1: 1: subtract the base function ReLU to implement only the non-linear part. 0: do nothing. -1: automatic;"
				,
				"",
				};
	

}
