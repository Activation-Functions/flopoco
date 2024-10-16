#ifndef INPUTIEEE_HPP
#define INPUTIEEE_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco{

	/** The InputIEEE class */
	class InputIEEE : public Operator
	{
	public:
		/**
		 * @brief The InputIEEE constructor
		 * @param[in]		target		the target device
		 * @param[in]		wE		the width of the exponent for the f-p number X
		 * @param[in]		wF		the width of the fraction for the f-p number X
		 */
		InputIEEE(OperatorPtr parentOp, Target* target, int wEI, int wFI, int wEO, int wFO, bool flushToZero=true);

		/**
		 * InputIEEE destructor
		 */
		~InputIEEE();

		// overloading functions from Operator
		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui);

		/** Generate unit tests */
		static TestList unitTest(int testLevel);

	private:
		/** The width of the exponent for the input X */
		int wEI;
		/** The width of the fraction for the input X */
		int wFI;
		/** The width of the exponent for the output R */
		int wEO;
		/** The width of the fraction for the output R */
		int wFO;
		/** used only when wEI>wEO: minimal exponent representable in output format, biased with input bias */
		int underflowThreshold;
		/** used only when wEI>wEO: maximal exponent representable in output format, biased with input bias */
		int overflowThreshold;
		/** if false, convert subnormals if possible (needs more hardware). If true, always flush them to zero */
		bool flushToZero;
	};

}
#endif //INPUTIEEE_HPP
