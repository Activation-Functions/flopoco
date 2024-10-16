/*
  A FIXME fast integer adder (design for long, unpipelined additions)
  for FloPoCo.

   Author: Bogdan Pasca

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

*/

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
#include <cmath>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "LongIntAdderAddAddMuxGen2.hpp"


using namespace std;
namespace flopoco{

	LongIntAdderAddAddMuxGen2::LongIntAdderAddAddMuxGen2(Target* target, int wIn, map<string, double> inputDelays, int regular):
		Operator(target), wIn_(wIn), inputDelays_(inputDelays)
	{
		srcFileName="LongIntAdderAddAddMuxGen2";
		setNameWithFreqAndUID(join("LongIntAdderAddAddMuxGen2_", wIn_));

		// Set up the IO signals
		for (int i=0; i<2; i++)
			addInput ( join("X",i) , wIn_, true);
		addInput("Cin");
		addOutput("R"  , wIn_, true, 1);

				//compute the maximum input delay
				maxInputDelay = getMaxInputDelays(inputDelays);


				if (false){
					// if (verbose)
					// 	cout << "The maximum input delay is "<<	maxInputDelay<<endl;

					cSize = new int[2000];
					REPORT(LogLevel::DEBUG, "-- The new version: direct mapping without 0/1 padding, IntAdders instantiated");
					double	objectivePeriod = double(1) / getTarget()->frequency();
					REPORT(LogLevel::VERBOSE, "Objective period is "<< objectivePeriod <<" at an objective frequency of "<<getTarget()->frequency());
					getTarget()->suggestSubaddSize(chunkSize_ ,wIn_);
					REPORT(LogLevel::VERBOSE, "The chunkSize for first two chunks is: " << chunkSize_ );

					if (2*chunkSize_ >= wIn_){
						cerr << "ERROR FOR NOW -- instantiate int adder, dimmension too small for LongIntAdderAddAddMuxGen2" << endl;
						exit(0);
					}

					cSize[0] = chunkSize_;
					cSize[1] = chunkSize_;

					bool finished = false; /* detect when finished the first the first
					phase of the chunk selection algo */
					int width = wIn_ - 2*chunkSize_; /* remaining size to split into chunks */
					int propagationSize = 0; /* carry addition size */
					int chunkIndex = 2; /* the index of the chunk for which the size is
					to be determined at the current step */
					bool invalid = false; /* the result of the first phase of the algo */

					/* FIRST PHASE */
					REPORT(LogLevel::DEBUG, "FIRST PHASE chunk splitting");
					while (not (finished))	 {
						REPORT(LogLevel::VERBOSE, "The width is " << width);
						propagationSize+=2;
						double delay = objectivePeriod - getTarget()->adderDelay(width)- getTarget()->adderDelay(propagationSize); //2*getTarget()->localWireDelay()  -
						REPORT(LogLevel::VERBOSE, "The value of the delay at step " << chunkIndex << " is " << delay);
						if ((delay > 0) || (width < 4)) {
							REPORT(LogLevel::VERBOSE, "finished -> found last chunk of size: " << width);
							cSize[chunkIndex] = width;
							finished = true;
						}else{
							REPORT(LogLevel::VERBOSE, "Found regular chunk ");
							int cs;
							double slack =  getTarget()->adderDelay(propagationSize) ; //+ 2*getTarget()->localWireDelay()
							REPORT(LogLevel::VERBOSE, "slack is: " << slack);
							REPORT(LogLevel::VERBOSE, "adderDelay of " << propagationSize << " is " << getTarget()->adderDelay(propagationSize) );
							getTarget()->suggestSlackSubaddSize( cs, width, slack);
							REPORT(LogLevel::VERBOSE, "size of the regular chunk is : " << cs);
							width = width - cs;
							cSize[chunkIndex] = cs;

							if ( (cSize[chunkIndex-1]<=2) && (cSize[chunkIndex-1]<=2) && ( invalid == false) ){
								REPORT(1, "[WARNING] Register level inserted after carry-propagation chain");
								invalid = true; /* invalidate the current splitting */
							}
							chunkIndex++; /* as this is not the last pair of chunks,
							pass to the next pair */
						}
					}
					REPORT(LogLevel::VERBOSE, "First phase return valid result: " << invalid);

					/* SECOND PHASE:
					only if first phase is cannot return a valid chunk size
					decomposition */
					if (invalid){
						REPORT(LogLevel::VERBOSE,"SECOND PHASE chunk splitting ...");
						getTarget()->suggestSubaddSize(chunkSize_ ,wIn_);
						lastChunkSize_ = (wIn_% chunkSize_ ==0 ? chunkSize_ :wIn_% chunkSize_);

						/* the index of the last chunk pair */
						chunkIndex = (wIn_% chunkSize_ ==0 ? ( wIn_ / chunkSize_) - 1 :  (wIn_-lastChunkSize_) / chunkSize_ );
						for (int i=0; i < chunkIndex; i++)
							cSize[i] = chunkSize_;
						/* last chunk is handled separately  */
						cSize[chunkIndex] = lastChunkSize_;
					}

					/* VERIFICATION PHASE: check if decomposition is correct */
					REPORT(LogLevel::VERBOSE, "found " << chunkIndex + 1  << " chunks ");
					nbOfChunks = chunkIndex + 1;
					int sum = 0;
					ostringstream chunks;
					for (int i=chunkIndex; i>=0; i--){
						chunks << cSize[i] << " ";
						sum+=cSize[i];
					}
					chunks << endl;
					REPORT(LogLevel::VERBOSE, "Chunks are: " << chunks.str());
					REPORT(LogLevel::VERBOSE, "The chunk size sum is " << sum << " and initial width was " << wIn_);
					if (sum != wIn_){
						cerr << "ERROR: check the algo" << endl; /*should never get here ... */
						exit(0);
					}
				}


				int ll,l0;
				// double xordelay;
				// double dcarry;
				// double muxcystoo;
				// double fdcq;
				double muxcystooOut;

				int fanOutWeight;

				if (getTarget()->getID()=="Virtex5"){
					// fdcq = 0.396e-9;
					// xordelay = 0.300e-9;
					// dcarry = 0.023e-9;
					// muxcystoo = 0.305e-9;
					muxcystooOut = 0.504e-9;
					fanOutWeight = 45;
				}else{
					if (getTarget()->getID()=="Virtex6"){
						// fdcq = 0.280e-9;
						// xordelay = 0.180e-9;
						// dcarry = 0.015e-9;
						// muxcystoo =	0.219e-9;
						muxcystooOut = 0.373e-9;
						fanOutWeight = 51;
					}else{
						if (getTarget()->getID()=="Virtex4"){
							// fdcq = 0.272e-9;
							// xordelay = 0.273e-9;
							// dcarry = 0.034e-9;
							// muxcystoo = 0.278e-9;
							muxcystooOut = 0.524e-9;
							fanOutWeight = 60;
						}
					}
				}
				int lkm1;


	double iDelay = getMaxInputDelays(inputDelays);

#ifdef MAXSIZE
	for (int aa=25; aa<=500; aa+=5){
		getTarget()->setFrequency(double(aa)*1000000.0);

#endif
bool nogo = false;
double t = 1.0 / getTarget()->frequency();

				if (!getTarget()->suggestSlackSubaddSize(lkm1, wIn, iDelay /*fdcq + getTarget()->localWireDelay()*/ + getTarget()->localWireDelay() + getTarget()->lutDelay())){
//					cerr << "Impossible 1" << endl;
					nogo = true;
				}
//				cout << "lkm1 = " << lkm1 << endl;

				double z =				iDelay +
										/*fdcq + getTarget()->localWireDelay() +*/
										getTarget()->lutDelay() + //xordelay +
										muxcystooOut + // the select to output line of the carry chain multiplexer.
													// usually this delay for the 1-bit addition which is not overlapping
										getTarget()->localWireDelay() +
										getTarget()->localWireDelay(fanOutWeight) + //final multiplexer delay. Fan-out of the CGC bits is accounted for
										getTarget()->lutDelay();
#ifdef DEBUGN
				cerr << "lut             delay = " << getTarget()->lutDelay() << endl;
				cerr << "muxcystooOut    delay = " << muxcystooOut << endl;
				cerr << "localWireDelay  delay = " << getTarget()->localWireDelay() << endl;
				cerr << "localWireDelay2 delay = " << getTarget()->localWireDelay(fanOutWeight) << endl;
				cerr << "z slack = " << z << endl;
#endif
				nogo = nogo | (!getTarget()->suggestSlackSubaddSize(ll, wIn, z));
#ifdef DEBUGN
				cerr << "ll is = "<<ll << endl;
#endif
				/*nogo = nogo | (!*/getTarget()->suggestSlackSubaddSize(l0, wIn, t - (2*getTarget()->lutDelay()+ muxcystooOut/* xordelay*/)); //);

				REPORT(LogLevel::DETAIL, "l0="<<l0);


				int maxAdderSize = lkm1 + ll*(ll+1)/2 + l0;
				if (nogo)
					maxAdderSize = -1;
				REPORT(LogLevel::DETAIL, "ll="<<ll);
				REPORT(LogLevel::DETAIL, "max adder size is="<< maxAdderSize);


#ifdef MAXSIZE
cout << " f="<<aa<<" s="<<maxAdderSize<<endl;
}
exit(1);
#endif
				cSize = new int[100];

				if (regular>0) {
					int c = regular;
					cout << "c="<<c<<endl;
					int s = wIn_;
					int j=0;
					while (s>0){
						if (s-c>0){
							cSize[j]=c;
							s-=c;
						}else{
							cSize[j]=s;
							s=0;
						}
						j++;
					}
					nbOfChunks = j;
				}else{
					int td = wIn;
					cSize[0] = l0;
					cSize[1] = 1;
					td -= (l0+1);
					nbOfChunks = 2;
					while (td > 0){
						int nc = cSize[nbOfChunks-1] + 1;
						int nnc = lkm1;

						REPORT(LogLevel::DETAIL,"nc="<<nc);
						REPORT(LogLevel::DETAIL,"nnc="<<nnc);

						if (nc + nnc >= td){
							REPORT(LogLevel::DETAIL, "Finish");
							//we can finish it now;
							if (nc>=td)
								nc = td-1;
							cSize[nbOfChunks] = nc;
							nbOfChunks++;
							td-=nc;
							cSize[nbOfChunks] = td;
							nbOfChunks++;
							td=0;
						}else{
							REPORT(LogLevel::DETAIL, "run");
							//not possible to finish chunk splitting now
							cSize[nbOfChunks] = nc;
							nbOfChunks++;
							td-=nc;
						}
					}
				}

				for (int i=0; i<nbOfChunks; i++)
					REPORT(LogLevel::DETAIL, "cSize["<<i<<"]="<<cSize[i]);

//#define test512
#ifdef test512
				nbOfChunks = 16;
				for (int i=1;i<=16;i++)
					cSize[i-1]=32;

#endif
				//=================================================
				//split the inputs ( this should be reusable )
				vhdl << tab << "--split the inputs into chunks of bits depending on the frequency" << endl;
				for (int i=0;i<2;i++)
					for (int j=0; j<nbOfChunks; j++){
						ostringstream name;
						//the naming standard: sX j _ i _ l
						//j=the chunk index i is the input index and l is the current level
						name << "sX"<<j<<"_"<<i<<"_l"<<0;
						int low=0, high=0;
						for (int k=0;k<=j;k++)
							high+=cSize[k];
						for (int k=0;k<=j-1;k++)
							low+=cSize[k];
						vhdl << tab << declare (name.str(),cSize[j],true) << " <=  X"<<i<<range(high-1,low)<<";"<<endl;
					}

				int l=1;
				for (int j=0; j<nbOfChunks; j++){
					//code for adder instantiation to stop ise from "optimizing"
					IntAdderSpecific *adder = new IntAdderSpecific(target, cSize[j]);

					if (j>0){ //for all chunks greater than zero we perform this additions
						inPortMap(adder, "X", join("sX",j,"_0_l",l-1) );
						inPortMap(adder, "Y", join("sX",j,"_1_l",l-1) );
						inPortMapCst(adder, "Cin", "'0'");
						outPortMap(adder, "R",    join("sX",j,"_0_l",l,"_Zero") );
						outPortMap(adder, "Cout", join("coutX",j,"_0_l",l,"_Zero") );
						vhdl << instance(adder, join("adderZ",j) );

						inPortMapCst(adder, "Cin", "'1'");
						outPortMap(adder, "R", join("sX",j,"_0_l",l,"_One"));
						outPortMap(adder, "Cout", join("coutX",j,"_0_l",l,"_One"));
						vhdl << instance( adder, join("adderO",j) );
					}else{
						vhdl << tab << "-- the carry resulting from the addition of the chunk + Cin is obtained directly" << endl;
						inPortMap(adder, "X", join("sX",j,"_0_l",l-1) );
						inPortMap(adder, "Y", join("sX",j,"_1_l",l-1) );
						inPortMapCst(adder, "Cin", "Cin");
						outPortMap(adder, "R",    join("sX",j,"_0_l",l,"_Cin") );
						outPortMap(adder, "Cout", join("coutX",j,"_0_l",l,"_Cin") );
						vhdl << instance(adder, join("adderCin",j) );
					}
				}

				vhdl << tab <<"--form the two carry string"<<endl;
				vhdl << tab << declare("carryStringZero",nbOfChunks-2) << " <= ";
				for (int i=nbOfChunks-3; i>=0; i--) {
					vhdl << "coutX"<<i+1<<"_0_l"<<l<<"_Zero"<< (i>0?" & ":";") ;
				} vhdl << endl;

				vhdl << tab << declare("carryStringOne",  nbOfChunks-2) << "  <= ";
				for (int i=nbOfChunks-3; i>=0; i--) {
					vhdl << "coutX"<<i+1<<"_0_l"<<l<<"_One" << " " << (i>0?" & ":";");
				} vhdl << endl;

				if (getTarget()->getVendor()== "Xilinx"){
					//////////////////////////////////////////////////////
					vhdl << tab << "--perform the short carry additions" << endl;
					CarryGenerationCircuit *cgc = new CarryGenerationCircuit(target,nbOfChunks-2);

					inPortMap(cgc, "X", "carryStringZero" );
					inPortMap(cgc, "Y", "carryStringOne" );
					inPortMapCst(cgc, "Cin", join("coutX",0,"_0_l",1,"_Cin"));
					outPortMap(cgc, "R",    "rawCarrySum" );
					vhdl << instance(cgc, "cgc");


					vhdl << tab <<"--get the final pipe results"<<endl;
					for ( int i=0; i<nbOfChunks; i++){
						if (i==0)
							vhdl << tab << declare(join("res",i),cSize[i],true) << " <= sX0_0_l1_Cin;" << endl;
						else {
							if (i==1) vhdl << tab << declare(join("res",i),cSize[i],true) << " <= " << join("sX",i,"_0_l",l,"_Zero") << " when " << join("coutX",0,"_0_l",l,"_Cin")<<"='0' else "<<join("sX",i,"_0_l",l,"_One")<<";"<<endl;
							else      vhdl << tab << declare(join("res",i),cSize[i],true) << " <= " << join("sX",i,"_0_l",l,"_Zero") << " when rawCarrySum"<<of(i-2)<<"='0' else "<<join("sX",i,"_0_l",l,"_One")<<";"<<endl;
						}
					}

				}else{ //Altera /////////////////////////////////////////////////////////////////////
					vhdl << tab << "--perform the short carry additions" << endl;
					IntAdderSpecific *cgc = new IntAdderSpecific(target,nbOfChunks-2);

					inPortMap(cgc, "X", "carryStringZero" );
					inPortMap(cgc, "Y", "carryStringOne" );
					inPortMapCst(cgc, "Cin", join("coutX",0,"_0_l",1,"_Cin"));
					outPortMap(cgc, "R",    "rawCarrySum" );
					outPortMap(cgc, "Cout", "cgcCout");
					vhdl << instance(cgc, "cgc");

					vhdl << tab <<"--get the final pipe results"<<endl;
					for ( int i=0; i<nbOfChunks; i++){
						if (i==0)
							vhdl << tab << declare(join("res",i),cSize[i],true) << " <= sX0_0_l1_Cin;" << endl;
						else {
							if (i==1) vhdl << tab << declare(join("res",i),cSize[i],true) << " <= " << join("sX",i,"_0_l",l,"_Zero") << " when " << join("coutX",0,"_0_l",l,"_Cin")<<"='0' else "<<join("sX",i,"_0_l",l,"_One")<<";"<<endl;
							else      vhdl << tab << declare(join("res",i),cSize[i],true) << " <= " << join("sX",i,"_0_l",l,"_One") << " when  ((not(rawCarrySum"<<of(i-2)<<") and carryStringOne"<<of(i-2)<<") or carryStringZero"<<of(i-2)<<")='1' else "<<join("sX",i,"_0_l",l,"_Zero")<<";"<<endl;
						}
					}
				}
				vhdl << tab << "R <= ";
				int k=0;
				for (int i=nbOfChunks-1; i>=0; i--){
					vhdl << join("res",i);
					if (i > 0) vhdl << " & ";
					k++;
				}
				vhdl << ";" <<endl;


			///////////////////////////////////////////////////////////////////////////////////////////////////////////////
			////////////////////////////////////////////////////////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////////////////////////////////////////////////////
			///////////////////////////////////////////////////////////////////////////////////////////////////////////////

	}

	LongIntAdderAddAddMuxGen2::~LongIntAdderAddAddMuxGen2() {
	}


	void LongIntAdderAddAddMuxGen2::emulate(TestCase* tc)
	{
		mpz_class svX[2];
		for (int i=0; i<2; i++){
			ostringstream iName;
			iName << "X"<<i;
			svX[i] = tc->getInputValue(iName.str());
		}
		mpz_class svC =  tc->getInputValue("Cin");

		mpz_class svR = svX[0] + svC;
		mpz_clrbit(svR.get_mpz_t(),wIn_);
		for (int i=1; i<2; i++){
			svR = svR + svX[i];
			mpz_clrbit(svR.get_mpz_t(),wIn_);
		}

		tc->addExpectedOutput("R", svR);

	}

	OperatorPtr LongIntAdderAddAddMuxGen2::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wIn;
		ui.parseStrictlyPositiveInt(args, "wIn", &wIn);
		int regular;
		ui.parseStrictlyPositiveInt(args, "regular", &regular);
		return new LongIntAdderAddAddMuxGen2(target, wIn, emptyDelayMap, regular);
	}

	void LongIntAdderAddAddMuxGen2::registerFactory(){
		UserInterface::add("LongIntAdderAddAddMuxGen2", // name
											 "An long integer adder.",
											 "ElementaryFunctions", // categories
											 "",
											 "wIn(int): input size in bits;\
regular(int)=0: force the chunk size",
											 "",
											 LongIntAdderAddAddMuxGen2::parseArguments
											 ) ;

	}

}
