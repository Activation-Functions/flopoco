#ifndef IEEEFPFMA_HPP
#define IEEEFPFMA_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"

namespace flopoco{

	/** The IEEEFPFMA class */
	class IEEEFPFMA : public Operator
	{
	public:
		/**
		 * The IEEEFPFMA constructor
		 * @param[in]		target		    target device
		 * @param[in]		wE			       the width of the exponent 
		 * @param[in]		wF			       the width of the fraction 
		 * @param[in]		ieee_compliant	 operate on IEEE numbers if true, on FloPoCo numbers if false 
		 */
		IEEEFPFMA(OperatorPtr parentOp, Target* target, int wE, int wF);

		/**
		 * IEEEFPFMA destructor
		 */
		~IEEEFPFMA();


		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		void buildRandomTestCases(TestCaseList* tcl, int n);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		static TestList unitTest(int testLevel);

	private:
		/** The width of the exponent */
		int wE; 
		/** The width of the fraction */
		int wF; 
	};

}

#endif
