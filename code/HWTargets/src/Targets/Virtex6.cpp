/*
  A model of Virtex-6 FPGA (FIXME exact part)

  Author : Bogdan Pasca

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.
*/


#include "flopoco/Targets/Virtex6.hpp"

using namespace std;

namespace flopoco{


	/** The default constructor. */
	Virtex6::Virtex6() : Target()	{
			id_             		= "Virtex6";
			vendor_         		= "Xilinx";
			possibleDSPConfig_.push_back(make_pair(25,18));
			whichDSPCongfigCanBeUnsigned_.push_back(false);
			sizeOfBlock_ 			= 36864;	// the size of a primitive block is 2^11 * 18 (36Kb, can be used as 2 independent 2^11*9)
			maxFrequencyMHz_		= 500;
			// all these values are set more or less randomly, to match  virtex 6 more or less
			fastcarryDelay_ 		= 0.015e-9; //s
			elemWireDelay_  		= 0.313e-9; // Was 0.313, but more recent versions of ISE report variable delays around 0.400
			lutDelay_       		= 0.053e-9;
			// all these values are set precisely to match the Virtex6
			fdCtoQ_         		= 0.280e-9;
			lut2_           		= 0.053e-9;	//the gate delay, without the NET delay (~0.279e-9)
			lut3_           		= 0.053e-9;
			lut4_           		= 0.053e-9;
			lut5_           		= 0.053e-9;
			lut6_           		= 0.053e-9;
			lut_net_                = 0.279e-9;
			muxcyStoO_      		= 0.219e-9;
			muxcyCINtoO_    		= 0.015e-9;
			ffd_            		= -0.012e-9;
			ff_net_            		= 0.604e-9;
			muxf5_          		= 0.291e-9;
			muxf7_          		= 0.187e-9; //without the NET delay (~0.357e-9)
			muxf7_net_         		= 0.357e-9;
			muxf8_          		= 0.131e-9; //without the NET delay (~0.481e-9)
			muxf8_net_         		= 0.481e-9;
			muxf_net_         		= 0.279e-9;
			slice2sliceDelay_   	= 0.393e-9;
			xorcyCintoO_    		= 0.180e-9;
			xorcyCintoO_net_ 		= 0.604e-9;

			lutInputs_ 				= 6;
			nrDSPs_ 				= 160;
			dspFixedShift_ 			= 17;

			DSPMultiplierDelay_		= 1.638e-9;
			DSPAdderDelay_			= 1.769e-9;
			DSPCascadingWireDelay_	= 0.365e-9;
			DSPToLogicWireDelay_	= 0.436e-9;

			RAMDelay_				= 1.591e-9; //TODO
			RAMToLogicWireDelay_	= 0.279e-9; //TODO
			logicWireToRAMDelay_	= 0.361e-9; //TODO

		}

	
	double Virtex6::logicDelay(int inputs){
		double delay;
		double unitDelay = lutDelay_ + elemWireDelay_;
		if(inputs <= lutInputs())
			delay = unitDelay;
		else if (inputs==lutInputs()+1) { // use mux
			delay=  muxf5_ + unitDelay; // TODO this branch never checked against reality
		}
		else if (inputs==lutInputs()+2) { // use mux
			delay=  muxf5_ + muxf7_ + muxf7_net_ + unitDelay; // TODO this branch never checked against reality
		}
		else
			delay= unitDelay * (inputs -lutInputs() + 1);
		TARGETREPORT("logicDelay(" << inputs << ") = " << delay*1e9 << " ns.");
		return delay;
	}
	
	
	double Virtex6::adderDelay(int size, bool addRoutingDelay) {
		double delay=lut2_ + muxcyStoO_ + double(size-1)*muxcyCINtoO_ + xorcyCintoO_ + elemWireDelay_;
		TARGETREPORT("adderDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay ;
	};

	

	double Virtex6::eqComparatorDelay(int size){
		double delay;
		delay= adderDelay((size+1)/2);
		TARGETREPORT("eqComparatorDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay;
	}

	double Virtex6::ltComparatorDelay(int size){
		double delay;
		delay= adderDelay((size+1)/2);
		TARGETREPORT("eqComparatorDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay;
	}

	double Virtex6::eqConstComparatorDelay(int size){
		double delay=  lut2_ + muxcyStoO_ + double((size-1)/lutInputs_+1)*muxcyCINtoO_  + xorcyCintoO_ + xorcyCintoO_net_;
		TARGETREPORT("eqConstComparatorDelay(" << size << ") = " << delay*1e9 << " ns.");
		return delay;
	}
	double Virtex6::ffDelay() {
		return fdCtoQ_ + ffd_; // removed ff_net_ 
	};

	double Virtex6::carryPropagateDelay() {
		return  fastcarryDelay_;
	};

	double Virtex6::fanoutDelay(int fanout){
		// TODO the values used here were found experimentally using Planahead 14.7
		double delay;
#if 0 // All this commented out by Florent to better match ISE critical path report
		// Then plugged back in because it really depends.
		if(fanout <= 16)
			delay =  elemWireDelay_ + (double(fanout) * 0.063e-9);
		else if(fanout <= 32)
			delay =  elemWireDelay_ + (double(fanout) * 0.035e-9);
		else if(fanout <= 64)
			delay =  elemWireDelay_ + (double(fanout) * 0.030e-9);
		else if(fanout <= 128)
			delay =  elemWireDelay_ + (double(fanout) * 0.017e-9);
		else if(fanout <= 256)
			delay =  elemWireDelay_ + (double(fanout) * 0.007e-9);
		else if(fanout <= 512)
			delay =  elemWireDelay_ + (double(fanout) * 0.004e-9);
		else
			delay =  elemWireDelay_ + (double(fanout) * 0.002e-9);
		delay/=2;
#else
			delay =  elemWireDelay_ + (double(fanout) * 0.001e-9);
#endif
		//cout << "localWireDelay(" << fanout << ") estimated to "<< delay*1e9 << "ns" << endl;
		TARGETREPORT("fanoutDelay(" << fanout << ") = " << delay*1e9 << " ns.");
		return delay;
	};

	double Virtex6::lutDelay(){
		return lutDelay_ ;
	};

	double Virtex6::tableDelay(int wIn_, int wOut_, bool logicTable_){
		double totalDelay = 0.0;
		int i;

		if(logicTable_)
		{
			if(wIn_ <= lutInputs_)
				//table fits inside a LUT
				totalDelay = fanoutDelay(wOut_) + lut6_ + lut_net_;
			else if(wIn_ <= lutInputs_+2){
				//table fits inside a slice
				double delays[] = {lut6_, muxf7_, muxf8_};

				totalDelay = fanoutDelay(wOut_);
				for(i=lutInputs_; i<=wIn_; i++){
					totalDelay += delays[i-lutInputs_];
				}
				totalDelay += muxf_net_;
			}else{
				//table requires resources from multiple slices
				double delays[] = {lut6_, 0, muxf7_};
				double delaysNet[] = {lut_net_, lut_net_, muxf7_net_};

				totalDelay = fanoutDelay(wOut_*(1<<(wIn_-lutInputs_))) + lut6_ + muxf7_ + muxf8_ + muxf8_net_;
				for(i=lutInputs_+3; i<=wIn_; i++){
					totalDelay += delays[(i-lutInputs_)%(sizeof(delays)/sizeof(*delays))];
				}
				i--;
				totalDelay += delaysNet[(i-lutInputs_)%(sizeof(delays)/sizeof(*delays))];
			}
		}else
		{
			totalDelay = RAMDelay_ + RAMToLogicWireDelay_;
		}

		return totalDelay;
	}

	int Virtex6::maxLutInputs() {
		return 8;
	}

	double Virtex6::lutConsumption(int lutInputSize)	{
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



	long Virtex6::sizeOfMemoryBlock()
	{
		return sizeOfBlock_;
	};

}
