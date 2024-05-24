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




			
					// let's do this one first to get the virtual bit heap etc right.
			if(getTarget()->plainVHDL())  // mostly to debug emulate() and interface
				{
					if(lsbPfull >= lsbOut) 			// Easy case when the multiplier needs no truncation
					{
						vhdl << declareFixPoint("P", signedIO, msbP, lsbPfull) << " <= XX*YY;" << endl;
						int newSizeP=msbOut -lsbPfull+1;
						vhdl << declareFixPoint("RP", signedIO, msbOut, lsbOut) << " <= " << (signedIO ? signExtend("P",  newSizeP) : zeroExtend("P", newSizeP))<< ";" << endl;
						//						cerr << "??? " << wA + msbOut-msbA << " " << signExtend("A", wA + msbOut-msbA) << " " << zeroExtend("A", wA+msbOut-msbA) << endl; 
						int newSizeA=msbOut -lsbA+1;
						vhdl << declareFixPoint("RA", signedIO, msbOut, lsbOut) << " <= " << typecast << "(" << (signedIO ? signExtend("AA", newSizeA) : zeroExtend("AA", newSizeA))<< ");" << endl;
				
						vhdl << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= RA+RP;" << endl;
				
					}
				else
					{ /////////////////// lsbPfull < lsbOut so we build a truncated multiplier

						vhdl << "RR <=0; -- This result is often wrong but it is computed quickly;  " << endl;
					}

				}

					
			else  // This is the bitheap-based version
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
					vhdl << tab << declareFixPoint("RR", signedIO, msbOut, lsbOut) << " <= " << typecast << "(" << bh.getSumName() << range(msbOut, lsbOut) << ");" << endl;
					
				}
			vhdl << "R <= std_logic_vector(RR);  " << endl;

		};
	
	
	
#if 0	// The old constructor for a stand-alone operator
	FixMultAdd::FixMultAdd(OperatorPtr parentOp, Target* target, Signal* x_, Signal* y_, Signal* a_,
												 int outMSB_, int outLSB_, bool enableSuperTiles_):
		Operator (parentOp, target),
		x(x_), y(y_), a(a_),
		outMSB(outMSB_),
		outLSB(outLSB_),
		wOut(outMSB_- outLSB_ + 1),
		enableSuperTiles(enableSuperTiles_)
	{

		srcFileName="FixMultAdd";
		setCopyrightString ( "Florent de Dinechin, Matei Istoan, 2012-2014, 2024" );

		// Set up the VHDL library style
		useNumericStd();

		wX = x->MSB() - x->LSB() +1;
		wY = y->MSB() - y->LSB() +1;
		wA = a->MSB() - a->LSB() +1;

		signedIO = (x->isSigned() && y->isSigned());
		// TODO: manage the case when one is signed and not the other.
		if((x->isSigned() && !y->isSigned()) || (!x->isSigned() && y->isSigned()))
		{
			THROWERROR("Different signs: x is "
								 << (x->isSigned()?"":"un")<<"signed and y is "
								 << (y->isSigned()?"":"un")<<"signed. This case is currently not supported." );
		}

		// THROWERROR("FixMultAdd needs more work");
		

#if 0
		
		// Set up the IO signals
		xname="X";
		yname="Y";
		aname="A";
		rname="R";

		// Determine the msb and lsb of the full product X*Y
		lsbP = x->LSB() + y->LSB();
		msbP = lsbP + (wX + wY + (signedIO ? -1 : 0)) - 1;

		//use 1 guard bit as default when we need to truncate A (and when either lsbP>aLSB or aMSB>msbP)
		g = ((a->LSB()<outLSB && lsbP>a->LSB() && lsbP>outLSB)
				|| (a->LSB()<outLSB && a->MSB()>msbP && msbP<outLSB)) ? 1 : 0;

		// Determine the actual msb and lsb of the product,
		// from the output's msb and lsb, and the (possible) number of guard bits
		//lsb
		if(((lsbP <= outLSB-g) && (g==1)) || ((lsbP < outLSB) && (g==0)))
		{
			//the result of the multiplication will be truncated
			workPLSB = outLSB-g;

			possibleOutputs = 2;
			REPORT(LogLevel::VERBOSE, "Faithfully rounded architecture");

			ostringstream dbgMsg;
			dbgMsg << "Multiplication of " << wX << " by " << wY << " bits, with the result truncated to weight " << workPLSB;
			REPORT(LogLevel::VERBOSE, dbgMsg.str());
		}
		else
		{
			//the result of the multiplication will not be truncated
			workPLSB = lsbP;

			possibleOutputs = 1; // No faithful rounding
			REPORT(LogLevel::VERBOSE, "Exact architecture");

			ostringstream dbgMsg;
			dbgMsg << "Full multiplication of " << wX << " by " << wY;
			REPORT(LogLevel::VERBOSE, dbgMsg.str());
		}
		//msb
		if(msbP <= outMSB)
		{
			//all msbs of the product are used
			workPMSB = msbP;
		}
		else
		{
			//not all msbs of the product are used
			workPMSB = outMSB;
		}

		//compute the needed guard bits and update the lsb
		if(msbP >= outLSB-g)
		{
			//workPLSB += ((g==1 && !(workPLSB==workPMSB && msbP==(outLSB-1))) ? 1 : 0);			//because of the default g=1 value
			g = IntMultiplier::neededGuardBits(target, wX, wY, workPMSB-workPLSB+1);
			workPLSB -= g;
		}

		// Determine the actual msb and lsb of the addend,
		// from the output's msb and lsb
		//lsb
		if(a->LSB() < outLSB)
		{
			//truncate the addend
			workALSB = (outLSB-g < a->LSB()) ? a->LSB() : outLSB-g;
			//need to correct the LSB of A (the corner case when, because of truncating A, some extra bits of X*Y are required, which are needed only for the rounding)
			if((a->LSB()<outLSB) && (a->MSB()>msbP) && (msbP<outLSB))
			{
				workALSB += (msbP==(outLSB-1) ? -1 : 0);
			}
		}
		else
		{
			//keep the full addend
			workALSB = a->LSB();
		}
		//msb
		if(a->MSB() <= outMSB)
		{
			//all msbs of the addend are used
			workAMSB = a->MSB();
		}
		else
		{
			//not all msbs of the product are used
			workAMSB = outMSB;
		}

		//create the inputs and the outputs of the operator
		addInput (xname,  wX);
		addInput (yname,  wY);
		addInput (aname,  wA);
		addOutput(rname,  wOut, possibleOutputs);

		//create the bit heap
		{
			ostringstream dbgMsg;
			dbgMsg << "Using " << g << " guard bits" << endl;
			dbgMsg << "Creating bit heap of size " << wOut+g << ", out of which " << g << " guard bits";
			REPORT(LogLevel::VERBOSE, dbgMsg.str());
		}
		bitHeap = new BitHeap(this,								//parent operator
							  wOut+g,							//size of the bit heap
							  enableSuperTiles);				//whether super-tiles are used

		//FIXME: are the guard bits included in the bits output by the multiplier?
		//create the multiplier
		//	this is a virtual operator, which uses the bit heap to do all its computations
		//cerr << "Before " << getCurrentCycle() << endl;
		if(msbP >= outLSB-g)
		{
			mult = new IntMultiplier(this,							//parent operator
															 target,
															 bitHeap,						//the bit heap that performs the compression
															 getSignalByName(xname),		//first input to the multiplier (a signal)
															 getSignalByName(yname),		//second input to the multiplier (a signal)
															 lsbP-(outLSB-g),				//offset of the LSB of the multiplier in the bit heap
															 false /*negate*/,				//whether to subtract the result of the multiplication from the bit heap
															 signedIO);
		}
		//cerr << "After " << getCurrentCycle() << endl;

		//add the addend to the bit heap
		int addendWeight;

		//if the addend is truncated, no shift is needed
		//	else, compute the shift from the output lsb
		//the offset for the signal in the bitheap can be either positive (signal's lsb < than bitheap's lsb), or negative
		//	the case of a negative offset is treated with a correction term, as it makes more sense in the context of a bitheap
		addendWeight = a->LSB()-(outLSB-g);
		//needed to correct the value of g=1, when the multiplier s truncated as well
		if((workALSB >= a->LSB()) && (g>1))
		{
			addendWeight += (msbP==(outLSB-1) ? 1 : 0);
		}
		//only add A to the bitheap if there is something to add
		if((a->MSB() >= outLSB-g) && (workAMSB>=workALSB))
		{
			if(signedIO)
			{
				bitHeap->addSignedBitVector(addendWeight,			//weight of signal in the bit heap
											aname,					//name of the signal
											workAMSB-workALSB+1,	//size of the signal added
											workALSB-a->LSB(),		//index of the lsb in the bit vector from which to add the bits of the addend
											(addendWeight<0));		//if we are correcting the index in the bit vector with a negative weight
			}
			else
			{
				bitHeap->addUnsignedBitVector(addendWeight,			//weight of signal in the bit heap
											  aname,				//name of the signal
											  workAMSB-workALSB+1,	//size of the signal added
											  (a->MSB()-a->LSB())-(workAMSB-a->MSB()),
																	//index of the msb in the actual bit vector from which to add the bits of the addend
											  workALSB-a->LSB(),	//index of the lsb in the actual bit vector from which to add the bits of the addend
											  (addendWeight<0));	//if we are correcting the index in the bit vector with a negative weight
			}
		}

		//correct the number of guard bits, if needed, because of the corner cases
		if((a->LSB()<outLSB) && (a->MSB()>msbP) && (msbP<outLSB))
		{
			if(msbP==(outLSB-1))
			{
				g++;
			}
		}

		//add the rounding bit
		if(g>0)
			bitHeap->addConstantOneBit(g-1);

		//compress the bit heap
		bitHeap -> generateCompressorVHDL();
		//assign the output
		vhdl << tab << rname << " <= " << bitHeap->getSumName() << range(wOut+g-1, g) << ";" << endl;
		
#endif

	}

#endif // Old constructor


	FixMultAdd::~FixMultAdd()
	{
		if(mult)
			delete mult;
#if 0 // Plug me back some day !
		if(plotter)
			delete plotter;
#endif
	}


	
#if 0 // This is probably useless now, and should be replaced with the standard interface
	FixMultAdd* FixMultAdd::newComponentAndInstance(
													Operator* op,
													string instanceName,
													string xSignalName,
													string ySignalName,
													string aSignalName,
													string rSignalName,
													int rMSB,
													int rLSB )
	{
		FixMultAdd* f = new FixMultAdd(
										 op->getTarget(),
										 op->getSignalByName(xSignalName),
										 op->getSignalByName(ySignalName),
										 op->getSignalByName(aSignalName),
										 rMSB, rLSB
										 );
		op->addSubComponent(f);

		op->inPortMap(f, "X", xSignalName);
		op->inPortMap(f, "Y", ySignalName);
		op->inPortMap(f, "A", aSignalName);

		op->outPortMap(f, "R", join(rSignalName,"_slv"));  // here rSignalName_slv is std_logic_vector
		op->vhdl << op->instance(f, instanceName);
		// hence a sign mismatch in next iteration.
		
		op->vhdl << tab << op->declareFixPoint(rSignalName, f->signedIO, rMSB, rLSB)
				<< " <= " <<  (f->signedIO ? "signed(" : "unsigned(") << (join(rSignalName, "_slv")) << ");" << endl;

		return f;
	}


#endif



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

		// our noble purpose here is to debug until the case 0*0+0 works
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
