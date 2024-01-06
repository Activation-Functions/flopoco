#ifndef FP2Fix_HPP
#define FP2Fix_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"


namespace flopoco{

	/** The FP2Fix class */
	class FP2Fix : public Operator
	{
	public:
		/**
		 * @brief The  constructor
		 * @param[in]		target		the target device
		 * @param[in]		MSB			the MSB of the output number for the conversion result
		 * @param[in]		LSB			the LSB of the output number for the conversion result
		 * @param[in]		wEI			the width of the exponent in input
		 * @param[in]		wFI			the width of the fraction in input
		 * @param[in]		trunc_p			the output is not rounded when trunc_p is true
		 */
		FP2Fix(Operator* parentOp, Target* target, int MSBO, int LSBO,  int wEI, int wFI, bool trunc_p);

		/**
		 * @brief destructor
		 */
	  ~FP2Fix();


	  void emulate(TestCase * tc);

		void buildStandardTestCases(TestCaseList* tcl);
		
	  // TestCase* buildRandomTestCase(int i);

		/** Generate unit tests */
		static TestList unitTest(int testLevel);


		/** Factory method that parses arguments and calls the constructor */
	  static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

	private:

	  /** The width of the exponent for the input */
	  int wE;
	  /** The width of the fraction for the input */
	  int wF;
	  /** The MSB for the output */
	  int MSB;
	  /** The LSB for the output */
	  int LSB;
	  bool trunc_p;
	  /** when true the output is truncated, when false it is rounded */


	};
}
#endif
