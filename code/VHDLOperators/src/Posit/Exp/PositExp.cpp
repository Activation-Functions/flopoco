/*
  An Posit exponential for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Oregane Desrentes, Oregane.Desrentes@ens-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/
#include <cmath> // For NaN
#include <fstream>
#include <iostream>
#include <sstream>

#include "flopoco/ConstMult/FixRealKCM.hpp"
#include "flopoco/FixFunctions/FixFunctionByPiecewisePoly.hpp"
#include "flopoco/FixFunctions/FixFunctionByTable.hpp"
#include "flopoco/IntAddSubCmp/IntAdder.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/Posit/Exp/PositExp.hpp"
#include "flopoco/ShiftersEtc/Shifters.hpp"
#include "flopoco/TestBenches/PositNumber.hpp"
#include "flopoco/utils.hpp"

using namespace std;



/* TODOs
Obtaining 400MHz in PositExp 8 23 depends on the version of ISE. Test with recent one.
remove the nextCycle after the multiplier

check the multiplier in the case 8 27: logic only, why?

Pass DSPThreshold to PolyEval

replace the truncated mult and following adder with an FixedMultAdd 
Clean up poly eval and bitheapize it

*/

#define LARGE_PREC 1000 // 1000 bits should be enough for everybody

namespace flopoco{

	vector<mpz_class>	PositExp::ExpYTable(int width, int wES)	{
	  vector<mpz_class> result;
	  	  
	  for (int x=0; x < (1<<width);x++) {
	    mpz_class h;
	    mpfr_t a,r;

	    mpz_class z(x);
	    PositNumber posit(width, wES, z);
	    mpfr_init2(a, LARGE_PREC);
	    mpfr_init2(r, LARGE_PREC);
	    posit.getMPFR(a); //pk la division dans FPEexp ? pas important psk pas en fixpoint ?
	    mpfr_exp(r, a, GMP_RNDN); //pas d'arrondi psk grands mpfr
	    PositNumber positres(width, wES, r); //là y'a l'arrondi
	    h = positres.getSignalValue();
	    mpfr_clears(a, r, NULL);
	    result.push_back(h);

	    //cout << x << " ok" << endl;
	  }
	  return result;
	};



	PositExp::PositExp(
			   OperatorPtr parentOp, Target* target, 
			   int width_, int wES_,
			   int k_, int d_, int guardBits, bool fullInput   
							 ):
		Operator(parentOp, target),
		width(width_), 
		wES(wES_), 
		k(k_), 
		d(d_), 
		g(guardBits)
	{
	        ostringstream name;
		name << "PositExp_" << width << "_" << wES ;
		setNameWithFreqAndUID(name.str());
		
		setCopyrightString("F. de Dinechin, Bogdan Pasca (2008-2019)");
		srcFileName="PositExp";
			
		bool expYTabulated=false;
		int sizeY;
		int sizeExpY;
		int blockRAMSize=getTarget()->sizeOfMemoryBlock();

		
		sizeY=width;
		sizeExpY = width;
		mpz_class sizeExpYTable= (mpz_class(1)<<sizeY) * sizeExpY;
		REPORT(LogLevel::DEBUG, "Tabulating e^Y would consume " << sizeExpYTable << " bits   (RAM block size is " << blockRAMSize << " bits");
		//if( sizeExpYTable <= mpz_class(blockRAMSize)) {
			REPORT(LogLevel::VERBOSE, "Tabulating e^Y in a blockRAM, using " << sizeExpYTable << " bits");
			expYTabulated=true;
			REPORT(LogLevel::VERBOSE, "g=" << g );
			REPORT(LogLevel::VERBOSE, "sizeY=" << sizeY);		
			REPORT(LogLevel::VERBOSE, "sizeExpY=" << sizeExpY);		
			//}

		addInput("X", width);
		addOutput("R", width);

		
		if(expYTabulated) {
			vector<mpz_class> expYTableContent = ExpYTable(width, wES); // e^A-1 has MSB weight 1
			TableOperator::newUniqueInstance(this, "X", "R",
						 expYTableContent,
						 "ExpYTable",
						 sizeY, sizeExpY);
		}
		
	}	

	PositExp::~PositExp()
	{
	}



	void PositExp::emulate(TestCase * tc)
	{

	}



	void PositExp::buildStandardTestCases(TestCaseList* tcl){
		
	}




         OperatorPtr PositExp::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
			int width, wES, k, d, g;
			ui.parseStrictlyPositiveInt(args, "width", &width); 
			ui.parsePositiveInt(args, "wES", &wES);
			ui.parsePositiveInt(args, "k", &k);
			ui.parsePositiveInt(args, "d", &d);
			ui.parseInt(args, "g", &g);
			return new PositExp(parentOp, target, width, wES, k, d, g);
		}

		template <>
		const OperatorDescription<PositExp> op_descriptor<PositExp> {
			"PositExp", // name
			"A faithful posit exponential function.",
			"Posit",
			"", // seeAlso
			"width(int): Posit size in bits; \
			wES(int): exponent size in bits;  \
			d(int)=0: degree of the polynomial. 0 choses a sensible default.; \
			k(int)=0: input size to the range reduction table, should be between 5 and 15. 0 choses a sensible default.;\
			g(int)=-1: number of guard bits",
			"Parameter d and k control the DSP/RamBlock tradeoff. "
			"In both cases, a value of 0 choses a sensible "
			"default. Parameter g is mostly for internal use.<br> "
			"For all the details, see <a "
			"href=\"bib/flopoco.html#DinechinPasca2010-FPT\">this "
			"article</a>."};
}





