#pragma once
#include <sstream>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>

#include "flopoco/BitHeap/BitHeap.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco
{

	/**
	 * The FixMultAdd class computes A+X*Y
	 * X*Y may be placed anywhere with respect to A;
	 * the product will be truncated when relevant.
	 * The result is specified as its LSB, MSB.
	 *
	 * Note on signed numbers:
	 * The bit heap manages signed addition in any case, so whether the addend is signed or not is irrelevant
	 * The product may be signed, or not.
	 */

	class FixMultAdd : public Operator {

	public:
		/**
		 * The FixMultAdd generic constructor computes x*y+a, faithful to msbOut.
		 **/
		FixMultAdd(OperatorPtr parentOp, Target* target,
							 bool signedIO,
							 int msbX, int lsbX,
							 int msbY, int lsbY,
							 int msbA, int lsbA,
							 int msbOut, int lsbOut);


		
		/**
		 *  Destructor
		 */
		~FixMultAdd();


		/**  Overloading Operator */
		void emulate ( TestCase* tc );

		/**  Overloading Operator */
		void buildStandardTestCases(TestCaseList* tcl);

		/**  Overloading Operator */
		static TestList unitTest(int index);
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		


	private:
		bool signedIO; /**<  signedness of the multiplicands */
		int msbX;     /**<  MSB position of the first multiplicand X */		
		int lsbX;			/**<	LSB position of the first multiplicand X */		
		int msbY;     /**<  MSB position of the second multiplicand Y */   
		int lsbY;     /**<	LSB position of the second multiplicand Y */  
		int msbA;     /**<  MSB position of the addend A*/ 
		int lsbA;     /**<	LSB position of the addend A*/ 
		int msbOut;   /**<  MSB position of the output signal */
		int lsbOut;   /**<  LSB position of the output signal */
		int wX;       /**< X input width */
		int wY;				/**< Y input width */
		int wA;				/**< A input width */
		int wOut;			/**< size of the result */
		int msbP;			/**< MSB of the product */
		int lsbPfull;	/**< lsb of the exact product */
		//		int lsbP;			/**< LSB of the truncated product */
		double maxAbsError;   /**< the max absolute value error of this multiplier, in ulps of the result. Should be 0 for untruncated, 1 or a bit less for truncated.*/
		bool isExact; /**< true if the operator involves no rounding */
		bool isCorrectlyRounded; /**< true if the operator involves rounding and rounding is to nearest (ties to up) */
		bool isFaithfullyRounded; /**< true if the operator involves rounding and rounding is faithful */
		

		// int g ;                    	/**< the number of guard bits if the product is truncated */
		// int maxWeight;             	/**< The max weight for the bit heap of this multiplier, wOut + g*/
		// int possibleOutputs;  		/**< 1 if the operator is exact, 2 if it is faithfully rounded */

	private:
		BitHeap* bitHeap;    		/**< The heap of weighted bits that will be used to do the additions */
		IntMultiplier* mult; 		/**< the virtual multiplier */
		Plotter* plotter;

		int workPMSB;				/**< MSB of the product, aligned with the output precision */
		int workPLSB;				/**< LSB of the product, aligned with the output precision */
		int workAMSB;				/**< MSB of the addend, aligned with the output precision */
		int workALSB;				/**< LSB of the addend, aligned with the output precision */

	};

} // namespace flopoco
