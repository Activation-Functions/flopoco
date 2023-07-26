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
#include "flopoco/FixFunctions/SNAFU.hpp"
#include "flopoco/IntMult/FixMultAdd.hpp"

using namespace std;




#define LARGE_PREC 1000 // 1000 bits should be enough for everybody


namespace flopoco{

	// constructor never called
	SNAFU::SNAFU(OperatorPtr parentOp, Target* target): Operator(parentOp, target) {}


	struct ActivationFunctionData {
		string fullDescription;
		string sollyaString;
		bool signedOutput;
		bool needsSlightRescale; // for output range efficiency of functions that touch 1 
		bool reluVariant; 
		
	};
		
	map<string, ActivationFunctionData> activationFunction = {
		{"sigmoid",
		 {
			 "Sigmoid",
			 "1/(1+exp(-X))", //textbook
			 false, // output unsigned
			 true, // output touches 1 and needs to be slightly rescaled
			 false // not a ReLU variant
		 }
		},    
		{"tanh",
		 {
			 "Hyperbolic Tangent",
			 "tanh(X)",  //textbook
			 true, // output signed
			 true, // output touches 1 and needs to be slightly rescaled
			 false // not a ReLU variant
			}
		},
		{"relu",
		 {
			 "Rectified Linear Unit",
			 "X/(1+exp(-1b256*X))", // Here we use a quasi-threshold function
			 false, // output unsigned
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 }
		},	
		{"elu",
		 {
			 "Exponential Linear Unit",
			 "X / (1 + exp(-1b256*X)) + expm1(X) * (1 - 1 / (1 + exp(-1b256*X)))", // Here we use a quasi-threshold function
			 true, // output signed
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 }
		},
		{"silu",
		 {
			 "Sigmoid Linear Unit",
			 "X / (1+exp(-X))",  // textbook
			 true, // output signed
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 },    				 
		}, 
		{"gelu",
		 {
			 "Gaussian Error Linear Unit",
			 "X/2*(1+erf(X/sqrt(2)))", // textbook
			 true, // output signed
			 false, // output does not need rescaling -- TODO CHECK
			 true // ReLU variant
		 }
		}
	};

	
	OperatorPtr SNAFU::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wIn, wOut;
		double inputScale;
		string fIn, methodIn;
		int adhocCompression;
		ui.parseString(args, "f", &fIn);
		ui.parseInt(args, "wIn", &wIn);
		ui.parseInt(args, "wOut", &wOut);
		ui.parseString(args, "method", &methodIn);
		ui.parseInt(args, "adhocCompression", &adhocCompression);
		ui.parseFloat(args, "inputScale", &inputScale);
		if(inputScale<=0){
			throw(string("inputScale should be strictly positive"));
		}
		OperatorPtr op;
		string f = to_lowercase(fIn);
		string method = to_lowercase(methodIn);

		
		auto af = activationFunction[f];
		string sollyaFunction = af.sollyaString;
		string functionDescription = af.fullDescription;
		bool signedOutput = af.signedOutput;
		bool needsSlightRescale = af.needsSlightRescale; // for output range efficiency of functions that touch 1 
		bool reluVariant = af.reluVariant; 

		//determine the LSB of the input and output
		int lsbIn = -(wIn-1); // -1 for the sign bit
		int lsbOut; // will be set below, it depends on signedness and if the function is a relu variant

		//////////////   Function preprocessing, for a good hardware match
		
		// scale the function by replacing X with (inputScale*x)
		string replaceString = "("+to_string(inputScale)+"*x)";
		size_t pos;
		while((pos=sollyaFunction.find("X")) != string::npos) {
				sollyaFunction.replace(pos, 1, replaceString);
			}
		
		// if necessary scale the output so the value 1 is never reached
		if(needsSlightRescale) {
			lsbOut = -wOut +(signedOutput?1:0);
			sollyaFunction = "(1-1b"+to_string(lsbOut)+") * ("+sollyaFunction+")";
		}
		else if (reluVariant) {
			lsbOut = -wOut + intlog2(inputScale);
		}
		else {
			throw(string("unable to set lsbOut"));
		}
		const bool signedIn=true;
		
		if (method=="auto") { // means "please choose for me"
			// TODO a complex sequence of if then else once the experiments are done
			method = "plaintable"; 
		}
		REPORT(LogLevel::MESSAGE, "Function after pre-processing: " << functionDescription << " evaluated on [-1,1)" << endl
					 << " wIn=" << wIn << " translates to lsbIn=" << lsbIn << endl
					 << " wOut=" << wOut << " translates to lsbOut=" << lsbOut);
		REPORT(LogLevel::MESSAGE, "Method is " << method);
		REPORT(LogLevel::MESSAGE, "To plot the function being implemented, copy-paste the following two lines in Sollya" << endl
					 << "  f = " << sollyaFunction << ";" << endl
					 << "  plot(f, [-1;1]); " );

		bool correctlyRounded=false; // default is faithful

		if(!adhocCompression) {
			if(method=="plaintable" && ) {
				op = new FixFunctionByTable(parentOp, target, sollyaFunction, signedIn, lsbIn, lsbOut);
				correctlyRounded=true;
			}
			else {
				throw(string("Method: ")+method + " currently unsupported");
			}
		}
		else {
			if (!reluVariant) {
				throw(string("Function ")+ fIn + " is not a ReLU variant, it doesn't make sense to apply adhocCompression");
			}
		}
		// sanity check
		if(int w = op->getSignalByName("X")->width() != wIn)  {
			REPORT(LogLevel::MESSAGE, "Something went wrong, operator input size is  " << w
						 << " instead of the requested wIn=" << wIn << endl << "Attempting to proceed nevertheless.");
		}
		if(int w = op->getSignalByName("Y")->width() != wOut)  {
			REPORT(LogLevel::MESSAGE, "Something went wrong, operator output size is  " << w
						 << " instead of the requested wOut=" << wOut << endl << "Attempting to proceed nevertheless.");
		}
		return op;
	}



	
		template <>
		const OperatorDescription<SNAFU> op_descriptor<SNAFU> {
	    "SNAFU", // name
				"Simple Neural Activation Function Unit, without reinventing the wheel",
				"FunctionApproximation",
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
