/*
  
  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA,
  2008-2014.
  All rights reserved.

*/
#ifndef __FIXHORNERARCHITECTURE_HPP
#define __FIXHORNERARCHITECTURE_HPP
#include <sstream>
#include <vector>

#include "flopoco/FixFunctions/BasicPolyApprox.hpp"
#include "flopoco/Target.hpp"


namespace flopoco{

	/** An Horner polynomial evaluator computing just right.
	 It assumes the input X is an signed number in [-1, 1[ so msbX=-wX.
	*/

  class FixHornerArchitecture
  {
  public:



		/** The constructor.
     * @param    lsbIn input lsb weight, 
			 @param    msbOut  output MSB weight, used to determine wOut
			 @param    lsbOut  output LSB weight
			 @param    poly the vector of polynomials that this evaluator should accomodate
			 @param   finalRounding: if false, the operator outputs its guard bits as well, saving the half-ulp rounding error. 
			                 This makes sense in situations that further process the result with further guard bits.
     */

																				
    FixHornerArchitecture(Target* target,
											 int lsbIn,
											 int msbOut,
											 int lsbOut,
											 std::vector<BasicPolyApprox*> p,
											 bool finalRounding=true);
		

  	Target& target;
  	std::vector<BasicPolyApprox*> poly;
   	int degree;                 /**< degree of the polynomial, extracted by the constructor */
	int lsbIn;                  /** LSB of input. Input is assumed in [0,1], so unsigned and MSB=-1 */
	int msbOut;                 /** MSB of output  */
	int lsbOut;                 /** LSB of output */
	bool finalRounding;         /** If true, the operator returns a rounded result (i.e. add the half-ulp then truncate)
								    if false, the operator returns the full, unrounded results including guard bits */
	// internal architectural parameters;
	std::vector<double> wcMaxAbsSum; /**< from 0 to degree */
	std::vector <int> wcSumSign; /**< 1: always positive; -1: always negative; 0: can be both  */  
	std::vector<int> wcSumMSB; /**< from 0 to degree */
	std::vector<int> wcSumLSB; /**< from 0 to degree */
	std::vector<int> wcYLSB; /**< from 0 to degree-1*/
	std::vector<bool> isZero; /*< a vector of size degree, true if all the coeffs of this degree are 0, avoids cornercase bugs*/
	void computeArchitecturalParameters(); /**< error analysis that ensures the rounding budget is met */
  };

}
#endif
