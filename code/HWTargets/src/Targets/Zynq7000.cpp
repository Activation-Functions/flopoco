/*
  A model of Zynq7000 FPGA (exact part: xc7z020clg484-1, used in the Zedboard) 

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2016.
  All rights reserved.
*/


#include "flopoco/Targets/Zynq7000.hpp"

using namespace std;

namespace flopoco{


	
	Zynq7000::Zynq7000(): Target()	{
			id_             		= "Zynq7000";
			vendor_         		= "Xilinx";

			maxFrequencyMHz_		= 500;

			/////// Architectural parameters
			lutInputs_ = 6;
			possibleDSPConfig_.push_back(make_pair(25,18));
			whichDSPCongfigCanBeUnsigned_.push_back(false);
			sizeOfBlock_ 			= 36864;	// the size of a primitive block is 2^11 * 9
			        // The blocks are 36kb configurable as dual 18k so I don't know.


			//////// Delay parameters, copypasted from Vivado timing reports
			
		}

	Zynq7000::~Zynq7000() {};

	//TODO
	double Zynq7000::logicDelay(int inputs){
		double delay;
		if(inputs <= lutInputs())
			delay= addRoutingDelay(lutDelay_);
		else
			delay= addRoutingDelay(lutDelay_) * (inputs -lutInputs() + 1);
		TARGETREPORT("logicDelay(" << inputs << ") = " << delay*1e9 << " ns.");
		return  delay; 
	}


	double Zynq7000::adderDelay(int size, bool addRoutingDelay_) {
		double delay = adderConstantDelay_ + ((size)/4 -1)* carry4Delay_;
		if(addRoutingDelay_) {
			delay=addRoutingDelay(delay);
			TARGETREPORT("adderDelay(" << size << ") = " << delay*1e9 << " ns.");
		}
		return  delay; 
	};

	
	double Zynq7000::eqComparatorDelay(int size){
		// Experiments using IntComparator
		return adderDelay(ceil(  ((double)size) / 3.0)); 
	}
	
	double Zynq7000::ltComparatorDelay(int size){
		// Experiments using IntComparator
		return adderDelay(ceil(  ((double)size) / 2.0)); 
	}
	
	double Zynq7000::eqConstComparatorDelay(int size){
		// TODO Refine
		return addRoutingDelay( lutDelay_ + double((size-1)/lutInputs_+1)/4*carry4Delay_ ); 
	}
	double Zynq7000::ffDelay() {
		return ffDelay_; 
	};
	
	double Zynq7000::addRoutingDelay(double d) {
		return(d+typicalLocalRoutingDelay_);
	};
	
	double Zynq7000::carryPropagateDelay() {
		return carry4Delay_/4 ; 
	};
	
	double Zynq7000::fanoutDelay(int fanout){

			/* Some data points from FPAdd 8 23, after synthesis  
				 net (fo=4, unplaced)         0.494     0.972    test/cmpAdder/X_d1_reg[33][1]
				 net (fo=102, unplaced)       0.412     3.377    test/cmpAdder/swap
				 net (fo=10, unplaced)        0.948     5.464    test/fracAdder/expDiff[6]
				 net (fo=15, unplaced)        0.472     6.261    test/cmpAdder/Y_d1_reg[29]_3
				 net (fo=2, unplaced)         1.122     7.507    test/cmpAdder/sticky_d1_i_29_n_0
				 net (fo=2, unplaced)         0.913     8.544    test/cmpAdder/sticky_d1_i_15_n_0
				 net (fo=1, unplaced)         0.902     9.570    test/cmpAdder/sticky_d1_i_37_n_0
				 net (fo=1, unplaced)         0.665    10.359    test/cmpAdder/sticky_d1_i_20_n_0
				 net (fo=2, unplaced)         0.913    11.396    test/cmpAdder/sticky_d1_i_6_n_0
	
				 The same, after place and route
				 
				 102		   2.138 
				 69	  		 2.901
				 24		  	 0.878
				 13			   1.103
				 6				 1.465
				 5  			 1.664
				 2				 0.841
				 2				 0.894
				 1				 0.640
				 C'est n'importe quoi.

			*/
		double delay= fanoutConstant_*(fanout); // and this is also completely random
		TARGETREPORT("fanoutDelay(" << fanout << ") = " << delay*1e9 << " ns.");
		return delay;
		
	}
	
	double Zynq7000::lutConsumption(int lutInputSize)
	{
		if (lutInputSize <= 5) {
			return .5;
		}
		switch (lutInputSize) {
			case 6:
				return 1.;
			case 7:
				return 2.;
			case 8:
				return 4.;
			default:
				return -1.;
		}
	}

	double Zynq7000::lutDelay(){
		return lutDelay_;
	}
	
	long Zynq7000::sizeOfMemoryBlock()
	{
		return sizeOfBlock_;	
	}
	
	double Zynq7000::tableDelay(int wIn, int wOut, bool logicTable){
		if(logicTable) {
			return logicDelay(wIn);
		}
		else {
			return RAMDelay_+ RAMToLogicWireDelay_;
		}
	}

}
