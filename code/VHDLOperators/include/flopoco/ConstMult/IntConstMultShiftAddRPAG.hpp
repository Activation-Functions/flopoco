/**
	Integer constant multiplication using minimum number of adders due to

	Gustafsson, O., Dempster, A., Johansson, K., Macleod, M., & Wanhammar, L. (2006).
	Simplified Design of Constant Coefficient Multipliers. Circuits, Systems, and Signal Processing, 25(2), 225–251.


	All constants up to 19 bit will be realized optimal using precomputed tables provided by the SPIRAL project (http://spiral.ece.cmu.edu/mcm/).

*/

#ifndef INTCONSTMULTOPT_HPP
#define INTCONSTMULTOPT_HPP

#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <cstdlib>
#include <cstdint> //for int64_t

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "IntConstMultShiftAdd.hpp"
#include "flopoco/UserInterface.hpp"

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)
#include "pagsuite/rpag.h"

namespace flopoco{

    class IntConstMultShiftAddRPAG : public IntConstMultShiftAdd
	{
	public:
		/** The standard constructor, inputs the number to implement */ 
        IntConstMultShiftAddRPAG(Operator* parentOp, Target* target, int wIn, list<mpz_class> &coeffList, bool syncInOut=true, int epsilon=0);

		static TestList unitTest(int index);
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
	private:
        stringstream adderGraph;
	};
}
#endif // Have paglib etc.

#endif
