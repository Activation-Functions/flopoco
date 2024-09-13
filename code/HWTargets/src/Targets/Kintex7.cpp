/*
  A model of Kintex7 FPGA (exact part: xc7k70tfbv484-3) 

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2016.
  All rights reserved.
*/


#include "flopoco/Targets/Kintex7.hpp"

using namespace std;

namespace flopoco{	
	Kintex7::Kintex7(): Target()	{
			id_             		= "Kintex7";
			vendor_         		= "Xilinx";

			maxFrequencyMHz_		= 741;

			/////// Architectural parameters
			lutInputs_ = 5;
			possibleDSPConfig_.push_back(make_pair(25,18));
			whichDSPCongfigCanBeUnsigned_.push_back(false);
			sizeOfBlock_ 			= 36864;	// the size of a primitive block is 2^11 * 9
			        // The blocks are 36kb configurable as dual 18k so I don't know.

			// See also all the constant parameters at the end of Kintex7.hpp
	}

	Kintex7::~Kintex7() {};

	double Kintex7::logicDelay(int inputs){
		double delay;
		do {
			if(inputs <= 5) {
				delay= addRoutingDelay(lut5Delay_);
				inputs=0;
			}
			else {
				delay=addRoutingDelay(lut6Delay_);
				inputs -= 6;
			}
		}
		while(inputs>0);
		TARGETREPORT("logicDelay(" << inputs << ") = " << delay*1e9 << " ns.");
		return  delay; 
	}


	double Kintex7::adderDelay(int size, bool addRoutingDelay_) {
		double delay = adderConstantDelay_ + ((size)/4 -1)* carry4Delay_;
		if(addRoutingDelay_) {
			delay=addRoutingDelay(delay);
			TARGETREPORT("adderDelay(" << size << ") = " << delay*1e9 << " ns.");
		}
		return  delay; 
	};

	
	double Kintex7::eqComparatorDelay(int size){
		return adderDelay((size+1)/3); 
	}

	double Kintex7::ltComparatorDelay(int size){
		return adderDelay((size+1)/2); 
	}
	
	double Kintex7::eqConstComparatorDelay(int size){
		// TODO refine
		return addRoutingDelay( lut5Delay_ + double((size-1)/lutInputs_+1)/4*carry4Delay_ ); 
	}

	double Kintex7::lutDelay(){
		return lut5Delay_;
	};

	double Kintex7::addRoutingDelay(double d) {
		return(d+ typicalLocalRoutingDelay_);
	};
		
	double Kintex7::fanoutDelay(int fanout){
		double delay= fanoutConstant_*fanout;
		TARGETREPORT("fanoutDelay(" << fanout << ") = " << delay*1e9 << " ns.");
		return delay;
	};
	
	double Kintex7::ffDelay() {
		return ffDelay_;
	};
	
	long Kintex7::sizeOfMemoryBlock()
	{
		return sizeOfBlock_;	
	};

	double Kintex7::lutConsumption(int lutInputSize) {
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

	double Kintex7::tableDelay(int wIn, int wOut, bool logicTable){
		if(logicTable) {
			return logicDelay(wIn);
		}
		else {
			return RAMDelay_;
		}
	}

	double Kintex7::adder3Delay(int size){return 0;}; // currently irrelevant for Xilinx
		double Kintex7::carryPropagateDelay(){return 0;};
		double Kintex7::DSPMultiplierDelay(){ 
			return DSPMultiplierDelay_;
		}
		double Kintex7::DSPAdderDelay(){
			// TODO: return DSPAdderDelay_;
			return 0;
		}
		double Kintex7::DSPCascadingWireDelay(){
			// return DSPCascadingWireDelay_;
			return 0;
		}
		double Kintex7::DSPToLogicWireDelay(){
			// return DSPToLogicWireDelay_;
			return 0;
		}





}




