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

#include "flopoco/FixFunctions/FixHorner.hpp"
#include "flopoco/FixFunctions/FixHornerEvaluator.hpp"
#include "flopoco/IntMult/FixMultAdd.hpp"

using namespace std;


	/*



		 This is a simplified version of the computation in the ASAP 2010 paper, simplified because x is in [-1,1)

		 S_d  =  a_d
		 S_i  =  a_i + x^C_i * S_{i+1}     \forall i \in \{ 0...d-1\}
		 p(y)      =  \sigma_0


		 2/  each Horner step may entail up to two errors: one when truncating X to Xtrunc, one when rounding/truncating the product. 
		 step i rounds to lsbMult[i], such that lsbMult[i] <=  polyApprox->LSB
		 Heuristic to determine lsbMult[i] is as follows:
		 - set them all to polyApprox->LSB.
		 - Compute the cumulated rounding error (as per the procedure below)
		 - while it exceeds the rounding error budget, decrease lsbMult[i], starting with the higher degrees (which will have the lowest area/perf impact)
		 - when we arrive to degree 1, increase again.

		 The error of one step is the sum of two terms: the truncation of X, and the truncation of the product.
		 The first is sometimes only present in lower-degree steps (for later steps, X may be used untruncated
		 For the second, two cases:
		 * If plainVHDL is true, we 
		   - get the full product, 
			 - truncate it to lsbMult[i]-1, 
			 - add it to the coefficient, appended with a rounding bit in position lsbMult[i]-1
			 - truncate the result to lsbMult[i], so we have effectively performed a rounding to nearest to lsbMult[i]:
			 epsilonMult = exp2(lsbMult[i]-1)
		 * If plainVHDL is false, we use a FixMultAdd faithful to lsbMult[i], so the mult rounding error is twice as high as in the plainVHDL case:
			 epsilonMult = exp2(lsbMult[i])


		 We still should consider the DSP granularity to truncate x to the smallest DSP-friendly size larger than |sigma_{j-1}|
		 TODO

a 		 So all that remains is to compute the parameters of the FixMultAdd.
		 We need
		 * size of \sigma_d: MSB is that of  a_{d-j}, plus 1 for overflows (sign extended).
							 LSB is lsbMult[i]

		 * size of x truncated x^T_i : 
		 * size of the truncated result:
		 * weight of the LSB of a_{d-j}
		 * weight of the MSB of a_{d-j}
		 */


#define LARGE_PREC 1000 // 1000 bits should be enough for everybody


namespace flopoco{




	FixHornerEvaluator::FixHornerEvaluator(OperatorPtr parentOp, Target* target, 
																				 int lsbIn_,
																				 int msbOut_,
																				 int lsbOut_,
																				 vector<BasicPolyApprox*> poly_,
																				 bool finalRounding):
		Operator(parentOp, target),
		Arch{target, lsbIn_, msbOut_, lsbOut_, poly_, finalRounding}
	{
		initialize();
		generateVHDL();
	}

	void FixHornerEvaluator::initialize(){
		setNameWithFreqAndUID("FixHornerEvaluator");		
		setCopyrightString("F. de Dinechin (2014-2020)");
		srcFileName="FixHornerEvaluator";
	}
	void FixHornerEvaluator::generateVHDL() {
		auto const & lsbIn = Arch.lsbIn;
		auto const & lsbOut = Arch.lsbOut;
		auto const & msbOut = Arch.msbOut;
		auto const & degree = Arch.degree; 
		addInput("Y", -lsbIn+1);
		vhdl << tab << declareFixPoint("Ys", true, 0, lsbIn) << " <= signed(Y);" << endl;
		for (int j=0; j<=degree; j++) {
			auto coeffPlaceholder = Arch.poly[0]->getCoeff(j);
			addInput(join("A",j), coeffPlaceholder->MSB-coeffPlaceholder->LSB +1);
		//TODO
		// convert the coefficients to signed. Remark: constant signs have been inserted by the caller
			vhdl << tab << declareFixPoint(join("As", j), true, coeffPlaceholder->MSB, coeffPlaceholder->LSB)
					 << " <= " << "signed(" << join("A",j) << ");" <<endl;
		}

		// declaring outputs
		addOutput("R", Arch.msbOut-Arch.lsbOut+1);

		// Initialize the Horner recurrence
		resizeFixPoint(join("S", degree), join("As", degree), Arch.wcSumMSB[degree], Arch.wcSumLSB[degree]);

		// Main loop of the Horner recurrence
		for(int i=degree-1; i>=0; i--) {
			resizeFixPoint(join("YsTrunc", i), "Ys", 0, Arch.wcYLSB[i]);

			//  assemble faithful operators (either FixMultAdd, or truncated mult)

			if(getTarget()->plainVHDL()) {	// no pipelining here
				int pMSB = 0 + Arch.wcSumMSB[i+1] + 1; // not attempting to save the MSB bit that could be saved.
				int pLSB = Arch.wcYLSB[i] + Arch.wcSumLSB[i+1];	
				vhdl << tab << declareFixPoint(join("P", i), true, pMSB,  pLSB)
						 <<  " <= "<< join("YsTrunc", i) <<" * S" << i+1 << ";" << endl;
				// Align before addition
				resizeFixPoint(join("Ptrunc", i), join("P", i), Arch.wcSumMSB[i], Arch.wcSumLSB[i]-1);
				if(Arch.isZero[i]) {
					vhdl << tab <<	declareFixPoint(join("Aext", i), true, Arch.wcSumMSB[i], Arch.wcSumLSB[i]) << " <= " <<  zg( Arch.wcSumMSB[i] -  Arch.wcSumLSB[i] +1) << ";" << endl; 
					}
				else {
					resizeFixPoint(join("Aext", i), join("As", i), Arch.wcSumMSB[i], Arch.wcSumLSB[i]-1); // -1 to make space for the round bit
				}

				vhdl << tab << declareFixPoint(join("SBeforeRound", i), true, Arch.wcSumMSB[i], Arch.wcSumLSB[i]-1)
						 << " <= " << join("Aext", i) << " + " << join("Ptrunc", i) << "+'1';" << endl;
				resizeFixPoint(join("S", i), join("SBeforeRound", i), Arch.wcSumMSB[i], Arch.wcSumLSB[i]);
			}

			else { // using FixMultAdd
				THROWERROR("Sorry, use the plainVHDL option until we revive FixMultAdd");
#if 0
				//REPORT(LogLevel::DEBUG, "*** iteration " << i << );
				FixMultAdd::newComponentAndInstance(this,
																						join("Step",i),     // instance name
																						join("XsTrunc",i),  // x
																						join("S", i+1), // y
																						join("As", i),       // a
																						join("S", i),   // result
																						wcSumMSB[i], wcSumLSB[i]
																						);
#endif
			}
		}
		if(Arch.finalRounding)
			resizeFixPoint("Rs", "S0",  msbOut, lsbOut);

		vhdl << tab << "R <= " << "std_logic_vector(Rs);" << endl;
	}
}
