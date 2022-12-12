#ifndef IEEEFPADD_HPP
#define IEEEFPADD_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"

namespace flopoco{

	/** The IEEEFPAdd class */
	class IEEEFPAdd : public Operator
	{
	public:
		/**
		 * The IEEEFPAdd constructor
		 * @param[in]		target		the target device
		 * @param[in]		wE			the width of the exponent
		 * @param[in]		wF			the width of the fraction
		 */
		IEEEFPAdd(OperatorPtr parentOp, Target* target, int wE, int wF, bool sub=false);

		/**
		 * IEEEFPAdd destructor
		 */
		~IEEEFPAdd();


		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		TestCase* buildRandomTestCase(int i);

		static TestList unitTest(int index);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

	private:
		/** The width of the exponent */
		int wE;
		/** The width of the fraction */
		int wF;
		/** Is this an FPAdd or an FPSub? */
		bool sub;

		int sizeRightShift;
		
		int sizeLeftShift;

	};

}

#endif
