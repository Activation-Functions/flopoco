#ifndef FPCOMPARATPOR_HPP
#define FPCOMPARATPOR_HPP

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/UserInterface.hpp"


namespace flopoco{
	class FPComparator : public Operator{
	public:
		/**
		 * The FPComparator constructor
		 * @param[in]		parentOp	parent operator in the instance hierarchy
		 * @param[in]		target		target device
		 * @param[in]		wE				with of the exponent
		 * @param[in]		wF				with of the fraction
		 */
		FPComparator(OperatorPtr parentOp, Target* target, int wE, int wF, int flags=31, int method=-1);

		/**
		 * FPComparator destructor
		 */
		~FPComparator();

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui);

		static TestList unitTest(int testLevel);

		/** emulate() function  */
		void emulate(TestCase * tc);


		/** Regression tests */
		void buildStandardTestCases(TestCaseList* tcl);

	private:
		/** bit width of the exponent */
		int wE;
		/** bit width of the fraction */
		int wF;
		/** cmpflags, see flopoco doc */
		int flags;
		/** method, see flopoco doc */
		int method;
	};
}
#endif
