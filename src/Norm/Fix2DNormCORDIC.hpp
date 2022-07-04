#ifndef Fix2DNormCORDIC_HPP
#define Fix2DNormCORDIC_HPP

#include "Operator.hpp"
#include "utils.hpp"

#include "Fix2DNorm.hpp"

namespace flopoco{ 

	
	class Fix2DNormCORDIC : public Fix2DNorm {
	  
	  public:
		/**
		 * The Fix2DNorm computes sqrt(x^2 + y^2) for x in [0, 1) and y in [0, 1).
		 * The MSB (Most significant bit) of both input is -1 whereas for the output it's 0.
		 *
		 * @param[in] target Target device
		 * @param[in] lsb    Least significant bit of both input and output
		 */
		Fix2DNormCORDIC(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut);

		// destructor
		~Fix2DNormCORDIC();

	private:
		mpfr_t kfactor;
		void initKFactor();

	  	/* TODO: Can it be computed from lsbIn and lsbOut ? */
		const int guardDiv = 4;
		int guardCordic;
		int maxIterations;
		void computeGuardBits();	  		

		void buildCordic();
		void buildKDivider();
	};
}

#endif
