#ifndef FLOPOCO_INTCONSTMULT_HPP
#define FLOPOCO_INTCONSTMULT_HPP

#include "flopoco/Operator.hpp"

/**
	@brief Integer constant multiplication.

	IntConstMult is the main interface to constant multipliers.
  It basically creates the instances of the highly optimized variants using shift and add or KCM

*/


namespace flopoco{

	class IntConstMult : public Operator
	{
	public:

		/** @brief The standard constructor, inputs the number to implement */
		IntConstMult(OperatorPtr parentOp, Target* target, const int wIn, const mpz_class constant, string method);

    void emulate(TestCase *tc);
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
  private:
    mpz_class constant;
    int wIn;
  };
}


#endif //FLOPOCO_INTCONSTMULT_HPP
