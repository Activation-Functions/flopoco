/*
   A multiply-and-add in a single bit heap

	Author:  Florent de Dinechin, Matei Istoan

	This file is part of the FloPoCo project

	Initial software.
	Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
	2012-2014.
	All rights reserved.
*/

/* TODO before there is any hope that it works:
	 - add to IntMultiplier the version that adds a multiplier to a bit heap: DONE
	 - and then more

	 a few command lines to test while developing:
	 ./flopoco fixmultadd signedio=0 msbx=3 lsbx=0 msby=3 lsby=0 msba=10 lsba=0 msbout=10 lsbout=0 testbench n=100

*/

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include "flopoco/IntMult/FixMultAdd.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

using namespace std;

namespace flopoco {



	FixMultAdd::FixMultAdd(OperatorPtr parentOp_, Target* target_,
												 bool signedIO_,
												 int msbX_, int lsbX_,
												 int msbY_, int lsbY_,
												 int msbA_, int lsbA_,
												 int msbOut_, int lsbOut_)
		: Operator (parentOp_, target_),
			signedIO(signedIO_),
			msbX(msbX_), lsbX(lsbX_),
			msbY(msbY_), lsbY(lsbY_),
			msbA(msbA_), lsbA(lsbA_),
			msbOut(msbOut_), lsbOut(lsbOut_)
		{
			srcFileName="FixMultAdd";
			setCopyrightString ( "Florent de Dinechin, Matei Istoan, 2012-2014, 2024" );
			// Set up the VHDL library style
			useNumericStd();

			// compute the sizes
			wX = msbX-lsbX+1;
			wY = msbY-lsbY+1;
			wA = msbA-lsbA+1;
			wOut = msbOut-lsbOut+1;
			// compute positions of the full product
			msbP = msbX+msbY+1; // always true, even though for signed numbers the MSB is nonzero only for one value: (-2^msbX) * (-2^msbY)
			lsbPfull = lsbX+lsbY; // this one is easy

			// A bit of sanity check on the IO formats
			if(msbP>msbOut) {
				THROWERROR("msbP is "<< msbP <<
									 " which is larger than the supplied msbOut " << msbOut <<
									 ", we don't do MSB truncation yet, sorry. Contact us if you have a use case.");
			}
			if(msbA>msbOut) {
				THROWERROR("msbA is "<< msbA <<
									 " which is larger than msbOut " << msbOut <<
									 ", we don't see in which situation it could make sense. Contact us if you have a use case.");
			}
			if(msbA==msbOut) {
				REPORT(LogLevel::MESSAGE, "Building a FixMultAdd that may overflow (msbA=msbOut), we hope you manage it");
			}
			if(msbP==msbOut) {
				REPORT(LogLevel::MESSAGE, "Building a FixMultAdd that may overflow (msbP=msbOut), we hope you manage it");
			}

			string typecast=signedIO?"signed":"unsigned";
			// Set the operator name
			ostringstream name;
			name <<"FixMultAdd";
			name << "_" << typecast;
			name << "_x_" << vhdlize(msbX) << "_" << vhdlize(lsbX);
			name << "_y_" << vhdlize(msbY) << "_" << vhdlize(lsbY);
			name << "_a_" << vhdlize(msbA) << "_" << vhdlize(lsbA);
			name << "_r_" << vhdlize(msbOut) << "_" << vhdlize(lsbOut);
			name << Operator::getNewUId();
			setNameWithFreqAndUID(name.str());
			REPORT(LogLevel::DEBUG, "Building " << name.str() << endl 
						 << "     X is " << (signedIO?"s":"u") << "fix(" << msbX <<    ", " << lsbX << ")" <<endl 
						 << "     Y is " << (signedIO?"s":"u") << "fix(" << msbY <<    ", " << lsbY << ")" <<endl 
						 << " Pfull is " << (signedIO?"s":"u") << "fix(" << msbP <<    ", " << lsbPfull << ")" <<endl 
						 << "     A is " << (signedIO?"s":"u") << "fix(" << msbA <<    ", " << lsbA << ")" <<endl 
						 << "     R is " << (signedIO?"s":"u") << "fix(" << msbOut <<    ", " << lsbOut << ")" <<endl 
						 );
		
			
			addInput ("X",  wX);
			addInput ("Y",  wY);
			addInput ("A",  wA);

			// Declaring the output
			if(lsbPfull >= lsbOut) 			// Easy case when the multiplier needs no truncation
				{
					isExact=true;
					isCorrectlyRounded=true; // no rounding will ever happen
					isFaithfullyRounded=true;// no rounding will ever happen
					addOutput("R",  wOut, 2); 
				}
			else{ /////////////////// lsbPfull < lsbOut so we build a truncated multiplier
				isExact=false;
				isCorrectlyRounded=false; //
				isFaithfullyRounded=true;// 
				addOutput("R",  wOut); 
			}

			// We cast all the inputs to fixed point, it makes life easier. Honest.
			vhdl << declareFixPoint("XX", signedIO, msbX, lsbX) << " <= " << typecast << "(X);" << endl;
			vhdl << declareFixPoint("YY", signedIO, msbY, lsbY) << " <= " << typecast << "(Y);" << endl;
			vhdl << declareFixPoint("AA", signedIO, msbA, lsbA) << " <= " << typecast << "(A);" << endl;




			if(getTarget()->plainVHDL())  // mostly to debug emulate() and interface
				{
					//For plainVHDL we even sometimes round to the nearest because we are lazy

					// Error analysis cases. lsbAdd is the size of the eventual adder.
					int lsbAdd = lsbOut-2; // works in any case, ie if both addend and product are truncated to lsbAdd
					if((lsbPfull<lsbOut-1 && lsbA>=lsbOut-1)  || (lsbPfull>=lsbOut-1 && lsbA<lsbOut-1) ) 	 
						{
							lsbAdd=lsbOut-1; 		// only one will be truncated with this lsbAdd
						}
					else if(lsbPfull>=lsbOut && lsbA>=lsbOut) // nobody needs truncation  
						{
							lsbAdd=min(lsbPfull,lsbA);
						}
						
					vhdl << declareFixPoint("Pfull", signedIO, msbP, lsbPfull) << " <= XX*YY;" << endl;
					// Now we have to possibly truncate and possibly extend both A and Pfull
					// Ptrunc and Atrunc manage the LSBs: either truncate with a range, or zero-extend to the right 
					vhdl << declareFixPoint("Ptrunc", signedIO, msbP, lsbAdd) << " <= Pfull" << (lsbAdd>lsbPfull? range(msbP-lsbPfull, lsbAdd-lsbPfull) : "") << (lsbAdd<lsbPfull? " & "+zg(lsbPfull-lsbAdd): "") << ";" << endl;
					int newSizeP = msbOut-lsbAdd+1;
					vhdl << declareFixPoint("Pext", signedIO, msbOut, lsbAdd) << " <= " << (signedIO ? signExtend("Ptrunc",  newSizeP) : zeroExtend("Ptrunc", newSizeP))<< ";" << endl;

					vhdl << declareFixPoint("Atrunc", signedIO, msbA, lsbAdd) << " <= AA" << (lsbAdd>lsbA? range(msbA-lsbA,lsbAdd-lsbA): "") << (lsbAdd<lsbA? " & "+zg(lsbA-lsbAdd): "") << ";" << endl;
					int newSizeA = msbOut-lsbAdd+1;
					vhdl << declareFixPoint("Aext", signedIO, msbOut, lsbAdd) << " <= " << (signedIO ? signExtend("Atrunc",  newSizeA) : zeroExtend("Atrunc", newSizeA))<< ";" << endl;

					if (lsbAdd==lsbOut-1)
						{// we may add the rounding bit for free
							vhdl << declareFixPoint("Sum", signedIO, msbOut, lsbOut-1) << " <= Aext+Pext + to_" << typecast << "(1, Sum'length); -- rounding bit" << endl;
							vhdl << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= Sum" << range(msbOut-lsbOut+1, 1) << ";" << endl;
						}
					if (lsbAdd==lsbOut-2)
						{// We add half a rounding bit, better than nothing but adding the rounding bit at the proper place would be more expensive 
							vhdl << declareFixPoint("Sum", signedIO, msbOut, lsbOut-1) << " <= Aext+Pext + to_" << typecast << "(1, Sum'length); --half rounding bit but it is already good" << endl;
							vhdl << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= Sum" << range(msbOut-lsbOut+2, 2) << ";" << endl;
						}
					if (lsbAdd==lsbOut)
						{// We add half a rounding bit, better than nothing but adding the rounding bit at the proper place would be more expensive 
							vhdl << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= Aext+Pext;" << endl;
						}
					if (lsbAdd>lsbOut)						
						{
							vhdl << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= (Aext+Pext) & "<< zg(lsbAdd-lsbOut) <<";" << endl;
						}
				}

					
			else  // This is the bitheap-based version. Look Ma, it is shorter!
				{
					// Or a fixed-point bit heap, not sure it has been much tested
					int lsbBH = min(lsbPfull, lsbA); // we will never create bits lower than that; some of these columns may remain empty
					BitHeap bh(this, msbOut, lsbBH);
					mpz_class multUlpError=0; // TODO an unsigned integer that measures the allowed mult error in ulps of the exact product
					IntMultiplier::addToExistingBitHeap(&bh,  "XX", "YY", multUlpError, lsbPfull); 
					bh.addSignal("AA", 0);
					if(!isExact) {
						bh.addConstantOneBit(lsbOut-1); // the round bit
					}
					bh.startCompression();
					vhdl << tab << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= " << typecast << "(" << bh.getSumName() << range(msbOut-lsbBH, lsbOut-lsbBH) << ");" << endl;
					
				}
			vhdl << "R <= std_logic_vector(RR);  " << endl;

		};
	
	
	

	FixMultAdd::~FixMultAdd()
	{
		if(mult)
			delete mult;
#if 0 // Plug me back some day !
		if(plotter)
			delete plotter;
#endif
	}


	



	// New emulate function better than in version 4.1: 
	// the inputs are fixed-point numbers, hence reals,
	// we build the corresponding MPFR numbers and perform the computation on them
	// This avoids replicating the logic of the VHDL as we had before.
	void FixMultAdd::emulate ( TestCase* tc )
	{
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");
		mpz_class svA = tc->getInputValue("A");
		// two's complement management)
		if(signedIO) {
			svX = bitVectorToSigned(svX, wX); 						// convert it to a signed mpz_class
			svY = bitVectorToSigned(svY, wY); 						// convert it to a signed mpz_class
			svA = bitVectorToSigned(svA, wA); 						// convert it to a signed mpz_class
		}
		
		// now build the corresponding mpfr numbers
		mpfr_t x, y, p, a, r,ru,rd;

		mpfr_init2 (x, wX);
		mpfr_set_z (x, svX.get_mpz_t(), MPFR_RNDD); 		// convert the integer svX to an MPFR; no rounding here
		mpfr_mul_2si (x, x, lsbX, MPFR_RNDD); 						// multiply this integer by 2^lsbX to obtain a fixed-point value; also exact

		mpfr_init2 (y, wY);
		mpfr_set_z (y, svY.get_mpz_t(), MPFR_RNDD); 				// convert the integer svX to an MPFR; no rounding here
		mpfr_mul_2si (y, y, lsbY, MPFR_RNDD); 						// multiply this integer by 2^lsbX to obtain a fixed-point value; also exact

		mpfr_init2 (a, wA);
		mpfr_set_z (a, svA.get_mpz_t(), MPFR_RNDD); 				// convert the integer svX to an MPFR; no rounding here
		mpfr_mul_2si (a, a, lsbA, MPFR_RNDD); 						// multiply this integer by 2^lsbX to obtain a fixed-point value; also exact

		mpfr_init2 (p, wX+wY); // enough bits for the exact product
		mpfr_mul(p, x , y, MPFR_RNDD); // should be exact
		int precForExact=max(msbOut, max(msbP, msbA)) - min(lsbOut, min(lsbPfull, lsbA)) + 2; 
		mpfr_init2 (rd, precForExact); 
		mpfr_init2 (ru, precForExact); 
		mpfr_init2 (r, precForExact);

		// There are valid use cases where a FixMultAdd may overflow on random tests but won't in its context.
		// we manage this by emulating the overflow (which depends on the signedness). 
		// This is probably disputable... anyway the following variables help for this. 
		mpz_class twotowR = mpz_class(1) << wOut;
		mpz_class minval = signedIO? (-(mpz_class(1)<<(wOut-1))) : mpz_class(0);
		mpz_class maxval = signedIO? ((mpz_class(1)<<(wOut-1))-1) : ((mpz_class(1)<<wOut)-1);

		if(isCorrectlyRounded){
			mpfr_add(r, a , p, MPFR_RNDN); // still no rounding here
			mpfr_mul_2si (r, r, -lsbOut, GMP_RNDN); // scaling back to integer: no rounding here
			mpz_class rz;
			mpfr_get_z (rz.get_mpz_t(), r, GMP_RNDN); 			// this is where rounding happens
			// emulate over/underflow
			while(rz >maxval) {	rz -= twotowR;	}
			while (rz < minval) {	rz += twotowR; } 

			if(signedIO) {
				rz = signedToBitVector(rz, wOut);
			}
			tc->addExpectedOutput ("R", rz);


		}
		else{  // FAITHFUL ROUNDING
			mpfr_add(rd, a , p, MPFR_RNDN); // no rounding here
			mpfr_add(ru, a , p, MPFR_RNDN); // no rounding here
			//		cerr << "(remove this annoying message) Size estimated to " << size << endl; 

			mpfr_mul_2si (rd, rd, -lsbOut, GMP_RNDN); // conversion back to integer: no rounding here
			mpfr_mul_2si (ru, ru, -lsbOut, GMP_RNDN); // conversion back to integer: no rounding here
			mpz_class rdz, ruz;
			mpfr_get_z (rdz.get_mpz_t(), rd, GMP_RNDD); 			// This is where rounding happens
			mpfr_get_z (ruz.get_mpz_t(), ru, GMP_RNDU); 			// This is where rounding happens
			// emulate overflow
			while(rdz > maxval) {	rdz -= twotowR;  }
			while(rdz < minval) {	rdz += twotowR; } 
			while(ruz > maxval) {	ruz -= twotowR;  }
			while(ruz < minval) {	ruz += twotowR; } 

			if(signedIO) {
				rdz = signedToBitVector(rdz, wOut);
				ruz = signedToBitVector(ruz, wOut);
			}

			tc->addExpectedOutput ("R", rdz);
			tc->addExpectedOutput ("R", ruz);
		}

		// free the memory
		mpfr_clears (x, y, a, p, r, ru, rd, NULL);
		
	}



	void FixMultAdd::buildStandardTestCases(TestCaseList* tcl) {
		TestCase *tc;

		// our ambitious goal here is to debug until the case 0*0+0 works
		tc = new TestCase(this); 
		tc->addInput("X", 0);
		tc->addInput("Y", 0);
		tc->addInput("A", 0);
		emulate(tc);
		tcl->add(tc);

		// ... and then the case 0*0 + ulp(A)		(a distant dream)
		tc = new TestCase(this); 
		tc->addInput("X", 0);
		tc->addInput("Y", 0);
		tc->addInput("A", 1);
		emulate(tc);
		tcl->add(tc);
	}

	
	OperatorPtr FixMultAdd::parseArguments(OperatorPtr parentOp, Target *target, std::vector<std::string> &args, UserInterface& ui) {
		bool signedIO;
		int msbX, lsbX, msbY, lsbY, msbA, lsbA, msbOut, lsbOut;
		ui.parseBoolean(args, "signedIO", &signedIO);
		ui.parseInt(args, "msbX", &msbX);
		ui.parseInt(args, "lsbX", &lsbX);
		ui.parseInt(args, "msbY", &msbY);
		ui.parseInt(args, "lsbY", &lsbY);
		ui.parseInt(args, "msbA", &msbA);
		ui.parseInt(args, "lsbA", &lsbA);
		ui.parseInt(args, "msbOut", &msbOut);
		ui.parseInt(args, "lsbOut", &lsbOut);
		return new FixMultAdd(parentOp,target,
													signedIO,
													msbX, lsbX,
													msbY, lsbY,
													msbA, lsbA,
													msbOut, lsbOut);
	}




	TestList FixMultAdd::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;

    list<vector<int>> params; // order: msbx lsbx msby lsby msba lsba msbout lsbout
		// (they will all be tested in 4 combinations of plainVHDL and signedIO)
    if(testLevel == TestLevel::QUICK)
    { // The quick tests
      params = {
				{3,0, 3,0, 10,0, 11,0}, // all exact integer without overflow
				{3,0, 3,0, 10,0, 10,0}, // all exact integer with overflow
				{3,-3, 3,-3, 10,0, 11,0}, // truncated mult, exact integer without overflow, result integer
				{3,-3, 3,-3, 10,0, 11,-1}, // truncated mult, exact addend, result half-integer
				{3,-3, 3,-3, 8,-2, 11,-1}, // truncated mult, truncated addend, result half-integer
				{3,0, 3,0, 8,-3, 11,-1}, // exact mult, truncated addend, result half-integer
				//				{3,-1, 3,0, 8,-3, 11,-1}, // exact mult, truncated addend, result half-integer
			};
    }
    else if(testLevel == TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
    }
    else if(testLevel >= TestLevel::EXHAUSTIVE)
    { // The substantial unit tests
    }

    for (auto param : params)
    {
      int msbx = param[0];
      int lsbx = param[1];
      int msby = param[2];
      int lsby = param[3];
      int msba = param[4];
      int lsba = param[5];
      int msbout = param[6];
      int lsbout = param[7];
      for(int plainvhdl=0; plainvhdl < 2; plainvhdl ++) // TODO update to test the bitheap version
				{
					for(int signedio=0; signedio < 2; signedio++)
						{
							paramList.push_back(make_pair("plainVHDL", plainvhdl ? "true" : "false"));
							paramList.push_back(make_pair("signedIO",   signedio ? "true" : "false"));
							paramList.push_back(make_pair("msbX", to_string(msbx)));
							paramList.push_back(make_pair("lsbX", to_string(lsbx)));
							paramList.push_back(make_pair("msbY", to_string(msby)));
							paramList.push_back(make_pair("lsbY", to_string(lsby)));
							paramList.push_back(make_pair("msbA", to_string(msba)));
							paramList.push_back(make_pair("lsbA", to_string(lsba)));
							paramList.push_back(make_pair("msbOut", to_string(msbout)));
							paramList.push_back(make_pair("lsbOut", to_string(lsbout)));
							testStateList.push_back(paramList);
							paramList.clear();
						}
				}
    }
		return testStateList;
	}


	
	template <>
	const OperatorDescription<FixMultAdd> op_descriptor<FixMultAdd> {
	    "FixMultAdd", // name
	    "A generic fixed-point A+X*Y. With so many parameters, it is probably mostly for internal use.",
	    "BasicInteger", // category
	    "",		    // see also
	    "signedIO(bool): signedness of multiplicands X and Y;\
		   msbX(int): position of the MSB of multiplicand X;\
		   lsbX(int): position of the LSB of multiplicand X;\
		   msbY(int): position of the MSB of multiplicand Y;\
		   lsbY(int): position of the LSB of multiplicand Y;\
		   msbA(int): position of the MSB of addend A;\
		   lsbA(int): position of the LSB of addend A;\
       msbOut(int): position of the MSB of output;\
		   lsbOut(int): position of the LSB of output", // This string will be parsed
	    ""};


}
