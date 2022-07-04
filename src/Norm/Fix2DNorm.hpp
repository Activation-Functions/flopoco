#ifndef Fix2DNorm_HPP
#define Fix2DNorm_HPP

#include "Operator.hpp"

namespace flopoco{ 

	
	class Fix2DNorm: public Operator {
	  
	  public:
		/**
		 * The Fix2DNorm computes sqrt(x^2 + y^2) for x in [0, 1) and y in [0, 1).
		 * The MSB (Most significant bit) of both input is -1 whereas for the output it's 0.
		 *
		 * @param[in] target Target device
		 * @param[in] lsbIn  Least significant bit of input
		 * @param[in] lsbOut Least significant bit of output
		 */
		Fix2DNorm(OperatorPtr parentOp, Target* target, int lsbIn, int lsbOut);

		/**
		 * The emulate function from Operator
		 */
		void emulate(TestCase* tc);

		void buildStandardTestCases(TestCaseList* tcl);
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */ 
		static void registerFactory();


	protected:		
		const int msbIn = -1;
	        int lsbIn;
		
		const int msbOut = 0;	
		int lsbOut;		
	  
		inline int getWOut() { return msbOut - lsbOut + 1; }
		inline int getWIn() { return msbIn - lsbIn + 1; }
	};
}

#endif
