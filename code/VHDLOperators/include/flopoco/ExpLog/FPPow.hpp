#ifndef __FPPOW_HPP
#define __FPPOW_HPP
#include <vector>
#include <sstream>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/ShiftersEtc/LZOC.hpp"
#include "flopoco/ShiftersEtc/Normalizer.hpp"
#include "flopoco/ShiftersEtc/Shifters.hpp"


namespace flopoco{


	class FPPow : public Operator
	{
	public:
		FPPow(OperatorPtr parentOp, Target* target, int wE, int wF, int type);
		~FPPow();

		void compute_error(mpfr_t & r, mpfr_t &epsE, mpfr_t& epsM, mpfr_t& epsL );

		//		Overloading the virtual functions of Operator
		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
        static TestList unitTest(int testLevel);
		/**Overloading the function of Operator */
		TestCase* buildRandomTestCase(int n);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		int wE, wF;

		int type;      /**< 0: pow; 1: powr */
	};
}
#endif
