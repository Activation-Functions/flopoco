#ifndef Fix2DNormLUT_HPP
#define Fix2DNormLUT_HPP

#include "Fix2DNorm.hpp"

namespace flopoco{ 
	
	class Fix2DNormLUT : public Fix2DNorm {	  
	  public:
		/**
		 * The Fix2DNormLUT computes sqrt(x^2 + y^2) for x in [0, 1) and y in [0, 1) using LUT.
		 * The MSB (Most significant bit) of both input is -1 whereas for the output it's 0.
		 *
		 * @param[in] target Target device
		 * @param[in] lsbIn  Least significant bit of input
		 * @param[in] lsbOut Least significant bit of output
		 */
		Fix2DNormLUT(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut);
	 private:
	};
}

#endif
