/*
  A model of Kintex7 FPGA (exact part: xc7k70tfbv484-3) 

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2016.
  All rights reserved.
*/


#include "flopoco/Targets/ManualPipeline.hpp"

using namespace std;

namespace flopoco{	
	ManualPipeline::ManualPipeline(): Target()	{
			id_             		= "ManualPipeline";
			vendor_         		= "Xilinx";

			maxFrequencyMHz_		= 741;

			/////// Architectural parameters
			lutInputs_ = 5;
			possibleDSPConfig_.push_back(make_pair(25,18));
			whichDSPCongfigCanBeUnsigned_.push_back(false);
			sizeOfBlock_ 			= 36864;	// the size of a primitive block is 2^11 * 9
			        // The blocks are 36kb configurable as dual 18k so I don't know.

			// See also all the constant parameters at the end of ManualPipeline.hpp
	}

	ManualPipeline::~ManualPipeline() {};

	double ManualPipeline::logicDelay(int inputs){
		return  0.; 
	}


	double ManualPipeline::adderDelay(int size, bool addRoutingDelay_) {
		return  0.; 
	};

	
	double ManualPipeline::eqComparatorDelay(int size){
		return  0.; 
	}

	double ManualPipeline::ltComparatorDelay(int size){
		return  0.; 
	}
	
	double ManualPipeline::eqConstComparatorDelay(int size){
		return  0.; 
	}
	double ManualPipeline::ffDelay() {
		return  0.; 
	};
	
	double ManualPipeline::addRoutingDelay(double d) {
		return  0.; 
	};
	
	
	double ManualPipeline::fanoutDelay(int fanout){
		return  0.; 
	};
	
	double ManualPipeline::lutDelay(){
		return  0.; 
	};
	
	long ManualPipeline::sizeOfMemoryBlock()
	{
		return  0.; 
	};

	double ManualPipeline::lutConsumption(int lutInputSize) {
		return  0.; 
	}

	double ManualPipeline::tableDelay(int wIn, int wOut, bool logicTable){
		return  0.; 
	}

	double ManualPipeline::cycleDelay() {
		return 1.1/frequency();
	}
	
	
	bool ManualPipeline::suggestSubaddSize(int &x, int wIn){
		return  wIn; 
	};


#if 0
	DSP* ManualPipeline::createDSP() 
	{
		int x, y;
		getMaxDSPWidths(x, y);
		
		/* create DSP block with constant shift of 17
		* and a maxium unsigned multiplier width (17x17) for this target
		*/
		DSP* dsp_ = new DSP(dspFixedShift_, x, y);
		
		return dsp_;
	};
#endif


#if 0	
	
	bool ManualPipeline::suggestSubmultSize(int &x, int &y, int wInX, int wInY){
		
		getMaxDSPWidths(x, y);
		
		//	//try the two possible chunk splittings
		//	int score1 = int(ceil((double(wInX)/double(x)))+ceil((double(wInY)/double(y))));
		//	int score2 = int(ceil((double(wInY)/double(x)))+ceil((double(wInX)/double(y))));
		//	
		//	if (score2 < score1)
		//		getMaxDSPWidths(y,x);		
		
		if (wInX <= x)
			x = wInX;
		
		if (wInY <= y)
			y = wInY;
		return true;
	}	 
	
	
	bool ManualPipeline::suggestSlackSubaddSize(int &x, int wIn, double slack){
		int chunkSize =  1 + (int)floor( (1./frequency() - slack - (lut2_ + muxcyStoO_ + xorcyCintoO_)) / muxcyCINtoO_ );
		x = chunkSize;	
		x = min(chunkSize, wIn);	
		if (x > 0) 
			return true;
		else {
			x = min(2,wIn);		
			return false;
		} 
	};
	
//	bool ManualPipeline::suggestSubaddSize(int &x, int wIn){
//		
//		int chunkSize = 1 + (int)floor( (1./frequency() - (lut2_ + muxcyStoO_ + xorcyCintoO_)) / muxcyCINtoO_ );
//		x = chunkSize;		
//		if (x > 1) 
//			return true;
//		else {
//			x = 2;		
//			return false;
//		} 
//	};
//	
//	bool ManualPipeline::suggestSlackSubaddSize(int &x, int wIn, double slack){
//		
//		int chunkSize = 1 + (int)floor( (1./frequency() - slack - (lut2_ + muxcyStoO_ + xorcyCintoO_)) / muxcyCINtoO_ );
//		x = chunkSize;		
//		if (x > 1) 
//			return true;
//		else {
//			x = 2;		
//			return false;
//		} 
//	};
//	
#endif
	bool ManualPipeline::suggestSlackSubcomparatorSize(int& x, int wIn, double slack, bool constant)
	{
		return  wIn; 
	}

#if 0
	int ManualPipeline::getIntMultiplierCost(int wInX, int wInY){
		
		int cost = 0;
		int halfLut = lutInputs_/2;
		int cx = int(ceil((double) wInX/halfLut));	// number of chunks on X
		int cy = int(ceil((double) wInY/halfLut));  // number of chunks on Y
		int padX = cx*halfLut - wInX; 				// zero padding of X input
		int padY = cy*halfLut - wInY; 				// zero padding of Y input
		
		if (cx > cy) // set cx as the min and cy as the max
		{
			int tmp = cx;
			cx = cy;
			cy = tmp;
			tmp = padX;
			padX = padY;
			padY = tmp;
		}
		
		float p = (double)cy/(double)halfLut; // number of chunks concatenated per operand
		float r = p - floor(p); // relative error; used for detecting how many operands have ceil(p) chunks concatenated
		int chunkSize, aux;
		suggestSubaddSize(chunkSize, wInX+wInY);
		// int lastChunkSize = (wInX+wInY)%chunkSize;
		// int nr = ceil((double) (wInX+wInY)/chunkSize);
		
		
		if (r == 0.0) // all IntNAdder operands have p concatenated partial products
		{
			aux = halfLut*cx; // number of operands having p concatenated chunks
			
			if (aux <= 4)	
				cost = p*lutInputs_*(aux-2)*(aux-1)/2-(padX*cx*(cx+1)+padY*aux*(aux+1))/2; // registered partial products without zero paddings
				else
					cost = p*lutInputs_*3 + p*lutInputs_*(aux-4)-(padX*(cx+1)+padY*(aux+1)); // registered partial products without zero paddings including SRLs
		}
		else if (r > 0.5) // 2/3 of the IntNAdder operands have p concatenated partial products
		{
			aux = (halfLut-1)*cx; // number of operands having p concatenated chunks
			
			if (halfLut*cx <= 4)
				cost = ceil(p)*lutInputs_*(aux-2)*(aux-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx-2)*(cx-1)/2);// registered partial products without zero paddings
				else
				{
					if (aux - 4 > 0)
					{
						cost = ceil(p)*lutInputs_*3 - padY - 2*padX + 	// registered partial products without zero paddings
						(ceil(p)*lutInputs_-padY) * (aux-4) + 	// SRLs for long concatenations
						(floor(p)*lutInputs_*cx - cx*padY); 		// SRLs for shorter concatenations
					}	
					else
					{
						cost = ceil(p)*lutInputs_*(aux-2)*(aux-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx+aux-6)*(cx+aux-5)/2); // registered partial products without zero paddings
						cost += (floor(p)*lutInputs_-padY) * (aux+cx-4); // SRLs for shorter concatenations
					}
				}
		}	
		else if (r > 0) // 1/3 of the IntNAdder operands have p concatenated partial products
		{
			aux = (halfLut-1)*cx; // number of operands having p concatenated chunks
			
			if (halfLut*cx <= 4)
				cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(aux-2)*(aux-1)/2);// registered partial products without zero paddings
				else
				{
					cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(aux-2)*(aux-1)/2);// registered partial products without zero paddings
					if (cx - 4 > 0)
					{
						cost = ceil(p)*lutInputs_*3 - padY - 2*padX; // registered partial products without zero paddings
						cost += ceil(p)*lutInputs_*(cx-4) - (cx-4)*padY; // SRLs for long concatenations
						cost += floor(p)*lutInputs_*aux - aux*padY; // SRLs for shorter concatenations
					}	
					else
					{
						cost = ceil(p)*lutInputs_*(cx-2)*(cx-1)/2 + floor(p)*lutInputs_*((aux*cx)+(cx+aux-6)*(cx+aux-5)/2); // registered partial products without zero paddings
						cost += (floor(p)*lutInputs_-padY) * (aux+cx-4); // SRLs for shorter concatenations
					}
				}
		}
		aux = halfLut*cx;
		cost += p*lutInputs_*aux + halfLut*(aux-1)*aux/2; // registered addition results on each pipeline stage of the IntNAdder
		
		if (padX+padY > 0)
			cost += (cx-1)*(cy-1)*lutInputs_ + cx*(lutInputs_-padY) + cy*(lutInputs_-padX);
		else
			cost += cx*cy*lutInputs_; // LUT cost for small multiplications 
			
			return cost*5/8;
	};
	
	
	int ManualPipeline::getEquivalenceSliceDSP(){
		int lutCost = 0;
		int x, y;
		getMaxDSPWidths(x,y);
		// add multiplier cost
		lutCost += getIntMultiplierCost(x, y);
		// add shifter and accumulator cost
		//lutCost += accumulatorLUTCost(x, y);
		return lutCost;
	}
	
	int ManualPipeline::getNumberOfDSPs() 
	{
		return nrDSPs_; 		
	};
	
	int ManualPipeline::getIntNAdderCost(int wIn, int n)
	{
		int chunkSize, lastChunkSize, nr, a, b, cost;
		
		suggestSubaddSize(chunkSize, wIn);
		lastChunkSize = wIn%chunkSize;
		nr = ceil((double) wIn/chunkSize);
		// IntAdder
		a = (nr-1)*nr*chunkSize/2 + (nr-1)*lastChunkSize;
		b = nr*lastChunkSize + (nr-1)*nr*chunkSize/2;
		
		if (nr > 2) // carry
		{
			a += (nr-1)*(nr-2)/2;
		}
		
		if (nr == 3) // SRL16E
		{
			a += chunkSize;	
		} 
		else if (nr > 3) // SRL16E, FDE
		{
			a += (nr-3)*chunkSize + 1;
			b += (nr-3)*chunkSize + 1;
		}
		// IntNAdder
		if (n >= 3)
		{
			a += (2*n-4)*wIn + 1;
			b += (2*n-5)*wIn;
		}
		
		cost = a+b*0.25;
		return cost;
	}
#endif

}
