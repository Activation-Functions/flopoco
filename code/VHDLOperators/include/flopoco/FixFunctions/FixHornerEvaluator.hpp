/*

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA,
  2008-2014.
  All rights reserved.

*/
#ifndef __FIXHORNEREVALUATOR_HPP
#define __FIXHORNEREVALUATOR_HPP

#include "flopoco/FixFunctions/FixHorner.hpp"
#include "flopoco/Operator.hpp"

#include <vector>

namespace flopoco
{

  /** An Horner polynomial evaluator computing just right.
	 It assumes the input X is an signed number in [-1, 1[ so msbX=-wX.
	*/

  class FixHornerEvaluator : public Operator {
    public:
    /** The constructor.
     * @param    lsbIn input lsb weight,
			 @param    msbOut  output MSB weight, used to determine wOut
			 @param    lsbOut  output LSB weight
			 @param    poly the vector of polynomials that this evaluator should accomodate
			 @param    finalRounding: if false, the operator outputs its guard bits as well, saving the half-ulp rounding error.
			                      This makes sense in situations that further process the result with further guard bits.
     */


    FixHornerEvaluator(
      OperatorPtr parentOp, Target* target, int lsbIn, int msbOut, int lsbOut, vector<BasicPolyApprox*> p, bool finalRounding = true);
    private:
    FixHornerArchitecture Arch;


    void initialize();   /**< initialization factored out between various constructors */
    void generateVHDL(); /**< generation of the VHDL once all the parameters have been computed */
  };

}  // namespace flopoco
#endif
