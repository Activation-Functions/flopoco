#ifndef Fix2DNorm_HPP
#define Fix2DNorm_HPP

#include "Operator.hpp"
#include "utils.hpp"

#include <vector>

namespace flopoco{ 

	
	class Fix2DNorm: public Operator {
	  
	  public:
		/**
		 * The Fix2DNorm computes sqrt(x^2 + y^2) for x in [0, 1) and y in [0, 1).
		 * The MSB (Most significant bit) of both input is -1 whereas for the output it's 0.
		 *
		 * @param[in] target Target device
		 * @param[in] lsb    Least significant bit of both input and output
		 */
		Fix2DNorm(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut);

		// destructor
		~Fix2DNorm();

		/**
		 * The emulate function from Operator
		 */
		void emulate(TestCase* tc);

		void buildStandardTestCases(TestCaseList* tcl);
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();


	private:
		/* TODO: Can it be computed from lsbIn and lsbOut ? */
		const int magic = 4;
		
		const int msbIn = -1;
	        int lsbIn;
		
		const int msbOut = 0;	
		int lsbOut;

		mpfr_t kfactor;
		void initKFactor();

		int guard;
		int maxIterations;
		void computeGuardBits();
	  
		inline int getWOut() { return msbOut - lsbOut + 1; }
		inline int getWIn() { return msbIn - lsbIn + 1; }

		void buildCordic();
		void buildKDivider();
	};
}

#endif
