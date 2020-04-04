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

#include "FixHornerEvaluator.hpp"
#include "IntMult/FixMultAdd.hpp"

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
		FixPolyEval(parentOp, target, lsbIn_, msbOut_, lsbOut_, poly_, finalRounding)
	{
		initialize();
		computeArchitecturalParameters();
		generateVHDL();
	}





	void FixHornerEvaluator::initialize(){
		setNameWithFreqAndUID("FixHornerEvaluator");		
		setCopyrightString("F. de Dinechin (2014-2020)");
		srcFileName="FixHornerEvaluator";
	}



	void FixHornerEvaluator::computeArchitecturalParameters(){
		// initialize the worst case parameters so that we can use array notation. All dummy values
		wcSumSign = vector<int>(degree+1, 17);
		wcMaxAbsSum = vector<double>(degree+1, -1);	
		wcSumMSB = vector<int>(degree+1,INT_MIN);
		wcSumLSB = vector<int>(degree+1,INT_MAX);
		wcYLSB = vector<int>(degree+1,INT_MAX);
		//		wcProductMSB = vector<int>(degree,0);
		//wcProductLSB = vector<int>(degree,0);

		sollya_obj_t yS = sollya_lib_build_function_free_variable();		
		sollya_obj_t rangeS = sollya_lib_parse_string("[-1;1]");		

		REPORT(INFO, "Entering computeArchitecturalParameters, for " << poly.size() << " intervals" );
	
		// iterate over all the polynomials to implement the error analysis on each interval
		for (size_t k=0; k<poly.size(); k++) {
			REPORT(INFO, "Error analysis on interval " << k  << " of " << poly.size()-1 );

			// First, compute the max abs value of the d intermediate sums
			vector<double> maxAbsSum(degree+1, -1);
			vector<int> sumMSB(degree+1, INT_MIN);
			// S_d=C_d in the ASA book
			sollya_obj_t sS =	sollya_lib_constant(poly[k] -> getCoeff(degree) -> fpValue);
			maxAbsSum[degree] = abs(mpfr_get_d(poly[k] -> getCoeff(degree) -> fpValue, MPFR_RNDN)); // should be round away from 0 but nobody will notice
			for(int i=degree-1; i>=0; i--) {
				int sumSign;
				sollya_obj_t cS = sollya_lib_constant(poly[k] -> getCoeff(i) -> fpValue);
				sollya_obj_t pS = sollya_lib_mul(yS, sS);
				sollya_lib_clear_obj(sS); // it has been used
				sS = sollya_lib_add(cS, pS);
				sollya_lib_clear_obj(pS); // it has been used
				sollya_lib_clear_obj(cS); // it has been used
				// REPORT(0, "interval " << k << ": expression of S_"<<i);
				// sollya_lib_printf("%b\n", sS);

				sollya_obj_t sIntervalS = sollya_lib_evaluate(sS,rangeS);
				sollya_obj_t supS = sollya_lib_sup(sIntervalS);
				sollya_obj_t infS = sollya_lib_inf(sIntervalS);
				mpfr_t supMP, infMP, tmp;
				mpfr_init2(supMP, 1000); // no big deal if we are not accurate here 
				mpfr_init2(infMP, 1000); // no big deal if we are not accurate here 
				mpfr_init2(tmp, 1000); // no big deal if we are not accurate here 
				sollya_lib_get_constant(supMP, supS);
				sollya_lib_get_constant(infMP, infS);

				if(mpfr_sgn(infMP) >=0 )
					sumSign = 1;
				else if(mpfr_sgn(supMP) <0 )
					sumSign = -1;
				else 
					sumSign = 0;
				
				// Now recompute the MSB explicitely.
				mpfr_abs(supMP, supMP, GMP_RNDU);
				mpfr_abs(infMP, infMP, GMP_RNDU);
				mpfr_max(supMP, infMP, supMP, GMP_RNDU); // now we have the supnorm
				maxAbsSum[i] = mpfr_get_d(supMP, GMP_RNDU);
				mpfr_log2(tmp, supMP, GMP_RNDU);
				mpfr_floor(tmp, tmp);
				sumMSB[i] = 1+ mpfr_get_si(tmp, GMP_RNDU); // 1+ because we assume signed arithmetic for s

				REPORT(INFO, " maxAbsSum[" << i << "] = " << maxAbsSum[i] << "  sumMSB[" << i << "] = " << sumMSB[i]);
				
				sollya_lib_clear_obj(sIntervalS);
				sollya_lib_clear_obj(supS);
				sollya_lib_clear_obj(infS);
				mpfr_clears(supMP,infMP,tmp, NULL);

				if(wcSumSign[i]==17)
					wcSumSign[i]=sumSign;
				else {
					if(sumSign!=wcSumSign[i])
						wcSumSign[i]=0; // otherwise leave it as it is
				} 
		
				// Finally update the worst-case values 
				wcSumMSB[i]     = max(wcSumMSB[i],     sumMSB[i]);
				//				wcSumLSB[i]     = min(wcSumLSB[i],     sumLSB);
				//wcProductMSB[i] = max(wcProductMSB[i], pMSB[i]);
				//wcProductLSB[i] = min(wcProductLSB[i], pLSB[i]);
			} // end loop on degree 
			// and free remaining memory
			sollya_lib_clear_obj(sS);

			
			REPORT(0, "OK, now we have the max Si, we may implement the error analysis: poly[k]->getApproxErrorBound()="<< poly[k]->getApproxErrorBound());
			// initialization
			double evalErrorBudget = exp2(lsbOut-1) - poly[k]->getApproxErrorBound();
			int lsb = lsbOut;
			vector<int> lsbY(degree,0);
			// we have delta_Y=2^lsbY, and we want to balance the error term maxS[i]deltaY with delta_multadd ~ 2^lsb,    
			for(int i=degree-1; i>=0; i--) {
				lsbY[i] = max(lsb - sumMSB[i], lsbIn);
			}

			bool evalErrorNotOK=true;
			while(evalErrorNotOK) {
				double evalerror = 0;
				for(int i=degree-1; i>=0; i--) {
					double multaddError=exp2(lsb);
					double yTruncationError = (lsbY[i]==lsbIn? 0 : exp2(lsbY[i])*maxAbsSum[i]); 
					evalerror += multaddError + yTruncationError;
				}
				if(evalerror < evalErrorBudget)
					evalErrorNotOK=false;
					
				REPORT(0, "Interval " << k  << "  evalErrorBudget=" << evalErrorBudget  << "   lsb=" << lsb
							 << " => evalError=" << evalerror << (evalErrorNotOK?":  increasing lsb... " : ":  OK!"));
				if(evalErrorNotOK) {
					lsb--;
					for(int i=degree-1; i>=0; i--) {
						lsbY[i] =  max(lsb - sumMSB[i], lsbIn);
					}	
				}
			}

			// OK, we have the lsbY and lsb for this polynomial, now update the worst-case
			for(int i=degree-1; i>=0; i--) {
				wcSumLSB[i] = min(wcSumLSB[i], lsb);
				wcYLSB[i] = min(wcYLSB[i], lsbY[i]);
			}	
			
		} // closes the for loop on k (the intervals)
 
		// Final reporting
		REPORT(0, "Architecture parameters:")
		for(int i=degree-1; i>=0; i--) {
			REPORT(0,"i=" << i << " YLSB=" << wcYLSB[i] << "  \t SumMSB=" << wcSumMSB[i] << "\t SumLSB=" << wcSumLSB[i] )
			}	
		

		sollya_lib_clear_obj(yS); 
		sollya_lib_clear_obj(rangeS);
	} 



	
#if 0
	void FixFunctionByPiecewisePoly::computeSigmaSignsAndMSBs(){
		mpfr_t res_left, res_right;
		sollya_obj_t rangeS;
		mpfr_init2(res_left, 10000); // should be enough for anybody
		mpfr_init2(res_right, 10000); // should be enough for anybody
		rangeS = sollya_lib_parse_string("[-1;1]");		

		REPORT(DEBUG, "Computing sigmas, signs and MSBs");

		// initialize the vector of MSB weights
		for (int j=0; j<=degree; j++) {
			sigmaMSB.push_back(INT_MIN);
			sigmaSign.push_back(17); // 17 meaning "not initialized yet"
		}
		
		for (int i=0; i<(1<<pwp -> alpha); i++){
			// initialize the vectors with sigma_d = a_d
			FixConstant* sigma = pwp -> poly[i] -> getCoeff(degree);
			sollya_obj_t sigmaS = sollya_lib_constant(sigma -> fpValue);
			int msb = sigma -> MSB;
			if (msb>sigmaMSB[degree])
				sigmaMSB[degree]=msb;
			sigmaSign[degree] = pwp -> coeffSigns[degree];
			
			for (int j=degree-1; j>=0; j--) {
				// interval eval of sigma_j
				// get the output range of sigma
				sollya_obj_t pi_jS = sollya_lib_mul(rangeS, sigmaS);
				sollya_lib_clear_obj(sigmaS);
				sollya_obj_t a_jS = sollya_lib_constant(pwp -> poly[i] -> getCoeff(j) -> fpValue);
				sigmaS = sollya_lib_add(a_jS, pi_jS);
				sollya_lib_clear_obj(pi_jS);
				sollya_lib_clear_obj(a_jS);
				// Get the endpoints
				int isrange = sollya_lib_get_bounds_from_range(res_left, res_right, sigmaS);
				if (isrange==false)
					THROWERROR("computeSigmaMSB: Not a range???");
				// First a tentative conversion to double to sort and get an estimate of the MSB and zeroness
				double l=mpfr_get_d(res_left, GMP_RNDN);
				double r=mpfr_get_d(res_right, GMP_RNDN);
				REPORT(DEBUG, "i=" << i << "  j=" << j << "  left=" << l << " right=" << r);
				// Now we want to know is if both have the same sign
				if (l>=0 && r>=0) { // sigma is positive
					if (sigmaSign[j] == 17 || sigmaSign[j] == +1)
						sigmaSign[j] = +1;
					else
						sigmaSign[j] = 0;
				}
				else if (l<0 && r<0) { // sigma is positive
					if (sigmaSign[j] == 17 || sigmaSign[j] == -1)
						sigmaSign[j] = -1;
					else
						sigmaSign[j] = 0;
				}
				else
					sigmaSign[j] = 0;
				

				// now finally the MSB computation
				int msb;
				mpfr_t mptmp;
				mpfr_init2(mptmp, 10000); // should be enough for anybody
				// we are interested in the max in magnitude
				if(fabs(l)>fabs(r)){
					mpfr_abs(mptmp, res_left, GMP_RNDN); // exact
				}
				else{
					mpfr_abs(mptmp, res_right, GMP_RNDN); // exact
				}
				mpfr_log2(mptmp, mptmp, GMP_RNDU);
				mpfr_floor(mptmp, mptmp);
				msb = mpfr_get_si(mptmp, GMP_RNDU);
				mpfr_clear(mptmp);
				msb++; // for the sign
				if (msb > sigmaMSB[j])
					sigmaMSB[j] = msb;
			} // for j
			
			
			sollya_lib_clear_obj(sigmaS);
		
		} // for i
		mpfr_clear(res_left);
		mpfr_clear(res_right);
		sollya_lib_clear_obj(rangeS);

		for (int j=degree; j>=0; j--) {
			REPORT(DETAILED, "Horner step " << j << ":   sigmaSign = " << sigmaSign[j] << " \t sigmaMSB = " << sigmaMSB[j]);
		}	

	}
#endif
	

	




	
	void FixHornerEvaluator::generateVHDL(){
		addInput("X", -lsbIn);
		vhdl << tab << declareFixPoint("Xs", true, 0, lsbIn) << " <= signed(X);" << endl;
		for (int j=0; j<=degree; j++) {
			addInput(join("A",j), coeffMSB[j]-coeffLSB[j] +1);
		}

		// declaring outputs
		addOutput("R", msbOut-lsbOut+1);
#if 0
		//		setCriticalPath( getMaxInputDelays(inputDelays) + target->localWireDelay() );


		// Split the coeff table output into various coefficients
		for(int i=0; i<=degree; i++) {
			vhdl << tab << declareFixPoint(join("As", i), signedXandCoeffs, msbCoeff[i], lsbCoeff)
					 << " <= " << (signedXandCoeffs?"signed":"unsigned") << "(" << join("A",i) << ");" <<endl;
		}

		// Initialize the Horner recurrence
		vhdl << tab << declareFixPoint(join("Sigma", degree), true, msbSigma[degree], lsbSigma[degree])
				 << " <= " << join("As", degree)  << ";" << endl;

		for(int i=degree-1; i>=0; i--) {
			resizeFixPoint(join("XsTrunc", i), "Xs", 0, lsbXTrunc[i]);

			//  assemble faithful operators (either FixMultAdd, or truncated mult)

			if(getTarget()->plainVHDL()) {	// no pipelining here
				vhdl << tab << declareFixPoint(join("P", i), true, msbP[i],  lsbP[i])
						 <<  " <= "<< join("XsTrunc", i) <<" * Sigma" << i+1 << ";" << endl;

				// Align before addition
				resizeFixPoint(join("Ptrunc", i), join("P", i), msbSigma[i], lsbSigma[i]-1);
				resizeFixPoint(join("Aext", i), join("As", i), msbSigma[i], lsbSigma[i]-1);

				vhdl << tab << declareFixPoint(join("SigmaBeforeRound", i), true, msbSigma[i], lsbSigma[i]-1)
						 << " <= " << join("Aext", i) << " + " << join("Ptrunc", i) << "+'1';" << endl;
				resizeFixPoint(join("Sigma", i), join("SigmaBeforeRound", i), msbSigma[i], lsbSigma[i]);
			}

			else { // using FixMultAdd
				THROWERROR("Sorry, use the plainVHDL option until we revive FixMultAdd");
#if 0
				//REPORT(DEBUG, "*** iteration " << i << );
				FixMultAdd::newComponentAndInstance(this,
																						join("Step",i),     // instance name
																						join("XsTrunc",i),  // x
																						join("Sigma", i+1), // y
																						join("As", i),       // a
																						join("Sigma", i),   // result
																						msbSigma[i], lsbSigma[i]
																						);
#endif
			}
		}
		if(finalRounding)
			resizeFixPoint("Ys", "Sigma0",  msbOut, lsbOut);

		vhdl << tab << "R <= " << "std_logic_vector(Ys);" << endl;

#endif
	}



	
#if 0

	// A naive constructor that does a worst case analysis of datapath looking onnly at the coeff sizes
	FixHornerEvaluator::FixHornerEvaluator(OperatorPtr parentOp, Target* target,
																				 int lsbIn_, int msbOut_, int lsbOut_,
																				 int degree_, vector<int> msbCoeff_, int lsbCoeff_,
																				 double roundingErrorBudget_,
																				 bool signedXandCoeffs_,
																				 bool finalRounding_)
	: Operator(parentOp, target), degree(degree_), lsbIn(lsbIn_), msbOut(msbOut_), lsbOut(lsbOut_),
		msbCoeff(msbCoeff_), lsbCoeff(lsbCoeff_),
		roundingErrorBudget(roundingErrorBudget_) ,signedXandCoeffs(signedXandCoeffs_),
		finalRounding(finalRounding_)
  {
		initialize();

		// initialize the vectors to the proper size so we can use them as arrays. I know.
		for (int i=0; i<degree; i++) {
			msbSigma.push_back(0);
			signSigma.push_back(0); // For signSigma this happens to be the default
			msbP.push_back(0);
			lsbP.push_back(0);
			lsbSigma.push_back(0);
			lsbXTrunc.push_back(0);
		}

		// Initialize the MSBs with a very rough analysis
		msbSigma[degree] = msbCoeff[degree];
		for(int i=degree-1; i>=0; i--) {
			msbSigma[i] = msbCoeff[i] + 1; // TODO this +1 is there for addition overflow, and probably overkill most of the times.
		}
		for(int i=degree-1; i>=0; i--) {
			msbP[i] = msbSigma[i+1] + 0 + 1;
		}

		// optimizing the lsbMults
		computeLSBs();

		generateVHDL();
  }


	// An optimized constructor if the caller has been able to compute the signs and MSBs of the sigma terms
	FixHornerEvaluator::FixHornerEvaluator(OperatorPtr parentOp, Target* target,
																				 int lsbIn_, int msbOut_, int lsbOut_,
																				 int degree_, vector<int> msbCoeff_, int lsbCoeff_,
																				 vector<int> sigmaSign_, vector<int> sigmaMSB_,
																				 double roundingErrorBudget_,
																				 bool signedXandCoeffs_,
																				 bool finalRounding_)
	: Operator(parentOp, target), degree(degree_), lsbIn(lsbIn_), msbOut(msbOut_), lsbOut(lsbOut_),
		msbCoeff(msbCoeff_), lsbCoeff(lsbCoeff_),
		roundingErrorBudget(roundingErrorBudget_) ,
		signedXandCoeffs(signedXandCoeffs_),
		finalRounding(finalRounding_),
		signSigma(sigmaSign_),  msbSigma(sigmaMSB_)
  {
		initialize();
		// initialize the vectors to the proper size so we can use them as arrays. I know.
		for (int i=0; i<degree; i++) {
			msbP.push_back(0);
			lsbP.push_back(0);
			lsbSigma.push_back(0);
			lsbXTrunc.push_back(0);
		}

		for(int i=degree-1; i>=0; i--) {
			msbP[i] = msbSigma[i+1] + 0 + 1;
		}


		// optimizing the lsbMults
		computeLSBs();

		generateVHDL();
  }

#endif
	
	FixHornerEvaluator::~FixHornerEvaluator(){}


}
