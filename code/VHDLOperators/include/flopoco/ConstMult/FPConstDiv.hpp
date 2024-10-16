#ifndef FPCONSTDIV_HPP
#define FPCONSTDIV_HPP
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "flopoco/Operator.hpp"
#include "IntConstDiv.hpp"


namespace flopoco{

	class FPConstDiv : public Operator
	{
	public:
		/** @brief The generic constructor */
		FPConstDiv(OperatorPtr parentOp, Target* target, int wEIn, int wFIn, int wEOut, int wFOut, vector<int> divisors, int dExp=0, int alpha=-1, int arch=0);


		~FPConstDiv();

		int wEIn;
		int wFIn;
		int wEOut;
		int wFOut;




		void emulate(TestCase *tc);
		void buildStandardTestCases(TestCaseList* tcl);
		/** Generate unit tests */
		static TestList unitTest(int testLevel);


		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		/** Factory register method */ 
		static void registerFactory();


	private:
		vector<int> divisors; /**< d is the produt of the divisors */
		int d; /**< The operator divides by d.2^dExp */
		int dExp;  /**< The operator divides by d.2^dExp */
		int alpha;
		bool mantissaIsOne;
		double dd; // the value of the actual constant in double: equal to d*2^dExp
		/// \todo replace the above with the mpd that we have in emulate
	};

}
#endif
