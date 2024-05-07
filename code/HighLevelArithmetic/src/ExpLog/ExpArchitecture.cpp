/*
Helper to chose the architecture for FP exponential for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/
#include <cmath> // For NaN
#include <fstream>
#include <iostream>
#include <sstream>
#include "flopoco/ExpLog/ExpArchitecture.hpp"
#include "flopoco/report.hpp"



using namespace std;


#define LARGE_PREC 1000 // 1000 bits should be enough for everybody

namespace flopoco{

	ExpArchitecture::ExpArchitecture(int blockRAMSize,
							 int wE_, int wF_,
							 int k_, int d_, int guardBits, bool fullInput, bool IEEEFPMode_
							 ):
		wE(wE_),
		wF(wF_),
		k(k_),
		d(d_),
		g(guardBits),
		IEEEFPMode(IEEEFPMode_)
	{


		/*  We have the following cases.

			 wF is really small. Then Y is small enough that e^Y can be
			 tabulated in a blockram.  In this case g=2.

			 10/11 < sizeY < ?? Y is still split into A and Z, but e^Z is simply
			 tabulated

			 ?? < sizeY <= 26 Y  is small enough that we can use the magic table
			 + 1-DSP reconstruction 3/
		*/

		this->expYTabulated=false;
		this->useTableExpZm1=false;
		this->useTableExpZmZm1=false;

		//* The following lines decide the architecture out of the size of wF *

		// First check if wF is small enough to tabulate e^Y in a block RAM
		if(guardBits==-1) // otherwise we don't touch it from the initialization
			this->g=2;
		sizeY=wF+g;
		sizeExpY = wF+g+1+2; // e^Y has MSB weight 1; 2 added because it enables to keep g=2 and it costs nothing here, being at the table output.
		mpz_class sizeExpATable= (mpz_class(1)<<sizeY) * sizeExpY;
		REPORT(LogLevel::DEBUG, "Tabulating e^Y would consume " << sizeExpATable << " bits   (RAM block size is " << blockRAMSize << " bits");
		if( sizeExpATable <= mpz_class(blockRAMSize)) {
			REPORT(LogLevel::VERBOSE, "Tabulating e^Y in a blockRAM, using " << sizeExpATable << " bits");
			expYTabulated=true;
			REPORT(LogLevel::VERBOSE, "g=" << g );
			REPORT(LogLevel::VERBOSE, "sizeY=" << sizeY);
			REPORT(LogLevel::VERBOSE, "sizeExpY=" << sizeExpY);
		}
		else if (wF<=23) {
			REPORT(LogLevel::VERBOSE, "We will split Y into A and Z, using a table for the Z part");
			if(guardBits==-1) // otherwise we don't touch it from the initialization
				g=4;
			k=10;
			sizeY=wF+g;
			sizeExpY = wF+g+1; // e^Y has MSB weight 1
			sizeExpA = sizeExpY;
			sizeZ = wF+g-k;
			sizeExpZm1 = sizeZ+1; //
			sizeMultIn = sizeZ; // sacrificing accuracy where it costs
			if (sizeZ<=k+1) { // Used to be k. k+1 prevents issue with table ExpZmZm1 being empty
				REPORT(LogLevel::VERBOSE, "Z is small, simpler table tabulating e^Z-1");
				useTableExpZm1=true;
			}
			else {
				REPORT(LogLevel::VERBOSE, "Z is large, magic table tabulating e^Z-Z-1");
				useTableExpZmZm1=true;
				sizeZtrunc=wF+g-2*k;
				sizeExpZmZm1 = wF+g - 2*k +1;
				sizeMultIn = sizeZ; // sacrificing accuracy where it costs
				REPORT(LogLevel::VERBOSE, "g=" << g);
				REPORT(LogLevel::VERBOSE, "k=" << k);
				REPORT(LogLevel::VERBOSE, "sizeY=" << sizeY);
				REPORT(LogLevel::VERBOSE, "sizeExpY=" << sizeExpY);
				REPORT(LogLevel::VERBOSE, "sizeZ=" << sizeZ);
				REPORT(LogLevel::VERBOSE, "sizeZtrunc=" << sizeZtrunc);
				REPORT(LogLevel::VERBOSE, "sizeExpZmZm1=" << sizeExpZmZm1);
				REPORT(LogLevel::VERBOSE, "sizeExpZm1=" << sizeExpZm1);
			}
		}

		else {// generic case
			if(guardBits==-1) // otherwise we don't touch it from the initialization
				g=4;
			if(k==0 && d==0) { 		// if automatic mode, set up the parameters
				REPORT(LogLevel::VERBOSE, "Chosing sensible defaults for both k and d");
				d=2;
				k=9;

				if (wF<32){
					d=1;
					k=9;
				}
				else if (wF<60) {
					d=2;
					k=10;
				}
				else if(wF<100) {
					d=3;
					k=11;
				}
				else if(wF<140) {
					d=4;
					k=12;
				}
			}
			else if(k!=0 && d==0) {
				// The idea here is that if k only was provided then we just do a single polynomial with no further table.
				d = max(wF/k-2, 0) ; // because Y<2^(-k) hence y^k<2^(-dk)
				REPORT(LogLevel::VERBOSE, "k=" << k << " provided, chosing sensible default for d: d="<<d);
			}
			else if(k==0 && d!=0) {
				k=9; // because it is always sensible?
				REPORT(LogLevel::VERBOSE, "d=" << d << " provided, chosing sensible default for k: k="<<k);
			}

			REPORT(LogLevel::VERBOSE, "Generic case with k=" << k << " and degree d=" << d);
			// redefine all the parameters because g depends on the branch
			sizeY=wF+g;
			sizeExpY = wF+g+1; // e^Y has MSB weight 1
			sizeExpA = sizeExpY;
			sizeZ = wF+g-k;
			sizeZtrunc=wF+g-2*k;
			sizeExpZmZm1 = wF+g - 2*k +1;
			sizeExpZm1 = sizeZ+1; //
			sizeMultIn = sizeZ; // sacrificing accuracy where it costs
			REPORT(LogLevel::VERBOSE, "k=" << k << " d=" << d);
			REPORT(LogLevel::VERBOSE, "g=" << g);
			REPORT(LogLevel::VERBOSE, "sizeY=" << sizeY);
			REPORT(LogLevel::VERBOSE, "sizeExpY=" << sizeExpY);
			REPORT(LogLevel::VERBOSE, "sizeZ=" << sizeZ);
			REPORT(LogLevel::VERBOSE, "sizeZtrunc=" << sizeZtrunc);
			REPORT(LogLevel::VERBOSE, "sizeExpZmZm1=" << sizeExpZmZm1);
			REPORT(LogLevel::VERBOSE, "sizeExpZm1=" << sizeExpZm1);
		}

		if (IEEEFPMode) {
			int Xmin = - floor( (-(1<<(wE-1))+2-wF) * log(2.0));
			MSB = floor(log2(Xmin-1));
		} else {
			MSB = wE-2;
		}

		LSB = -wF-g;

	}

	ExpArchitecture::~ExpArchitecture()
	{
	}

	int ExpArchitecture::getd() { return this->d; }
	int ExpArchitecture::getg() { return this->g; }
	int ExpArchitecture::getk() { return this->k; }
	int ExpArchitecture::getMSB() { return this->MSB; }
	int ExpArchitecture::getLSB() { return this->LSB; }
	bool ExpArchitecture::getExpYTabulated() { return this->expYTabulated; }
	bool ExpArchitecture::getUseTableExpZm1() { return this->useTableExpZm1; }
	bool ExpArchitecture::getUseTableExpZmZm1() { return this->useTableExpZmZm1; }
	bool ExpArchitecture::getIEEEFPMode() { return this->IEEEFPMode; }
	int ExpArchitecture::getSizeY() { return this->sizeY; }
	int ExpArchitecture::getSizeZ() { return this->sizeZ; }
	int ExpArchitecture::getSizeExpY() { return this->sizeExpY; }
	int ExpArchitecture::getSizeExpA() { return this->sizeExpA; }
	int ExpArchitecture::getSizeZtrunc() { return this->sizeZtrunc; }
	int ExpArchitecture::getSizeExpZmZm1() { return this->sizeExpZmZm1; }
	int ExpArchitecture::getSizeExpZm1() { return this->sizeExpZm1; }
	int ExpArchitecture::getSizeMultIn() { return this->sizeMultIn; }

} // namespace flopoco
