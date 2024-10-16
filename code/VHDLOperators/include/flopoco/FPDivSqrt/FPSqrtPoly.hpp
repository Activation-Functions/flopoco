#ifndef FPSqrtPoly_HPP
#define FPSqrtPoly_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "flopoco/Operator.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"
#include "flopoco/FixedPointFunctions/PolyCoeffTable.hpp"
#include "flopoco/FixedPointFunctions/PolynomialEvaluator.hpp"
#include "flopoco/squareroot/PolynomialTableCorrectRounded.hpp"
#include "flopoco/squareroot/PolynomialTable.hpp"

namespace flopoco{
	/** The FPSqrtPoly class */
	class FPSqrtPoly : public Operator
	{
	public:
		/**
		 * The FPSqrtPoly constructor
		 * @param[in]		target		the target device
		 * @param[in]		wE			the width of the exponent for the f-p number X
		 * @param[in]		wF			the width of the fraction for the f-p number X
		 * @param[in]  correctlyRounded if the result will be correctly rounded or not
		 */
		FPSqrtPoly(Target* target, int wE, int wF, bool correctlyRounded = false, int degree = 2);

		/**
		 * FPSqrtPoly destructor
		 */
		~FPSqrtPoly();

		/**
		 * Emulate a correctly rounded square root using MPFR.
		 * @param tc a TestCase partially filled with input values
		 */
		void emulate(TestCase * tc);

		/* Overloading the Operator method to limit testing of NaNs and negative numbers*/
		TestCase* buildRandomTestCase(int i);
		//	void buildStandardTestCases(TestCaseList* tcl);
	private:
		/** The width of the exponent for the input X */
		int wE;
		/** The width of the fraction for the input X */
		int wF;

		/** A boolean selecting between IEEE-compliant correct rounding
			 or faithful (last-bit accurate) result  */
		bool correctRounding;
	}
		;
}
#endif //FPSqrtPoly_HPP
