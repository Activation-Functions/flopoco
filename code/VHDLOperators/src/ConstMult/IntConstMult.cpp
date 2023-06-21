#include "flopoco/ConstMult/IntConstMult.hpp"
#include "flopoco/InterfacedOperator.hpp"

using namespace std;

namespace flopoco{

IntConstMult::IntConstMult(OperatorPtr parentOp, Target* target, int wIn, mpz_class constant) : Operator(parentOp, target), wIn(wIn), constant(constant)
{
  srcFileName="IntConstMult";

  ostringstream name;
  name << "IntConstMult_" << wIn;
  setNameWithFreqAndUID(name.str());


  int wOut = intlog2(constant * ((mpz_class(1)<<wIn)-1));

  addInput("X", wIn);
  addOutput("R", wOut);

  declare("R_tmp",wOut);
  newInstance("IntConstMultShiftAddPlain",
            "IntConstMultShiftAddPlain",
            "wIn=" + std::to_string(wIn) + " constant=" + mpz2string(constant),
            "X=>X",
            "R=>R_tmp");

  vhdl << tab << "R <= R_tmp;" << endl;

}

void flopoco::IntConstMult::emulate(TestCase *tc){
  mpz_class X = tc->getInputValue("X");
  mpz_class R = X * constant;
  tc->addExpectedOutput("R", R);
}


OperatorPtr flopoco::IntConstMult::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
{
  int wIn;

  ui.parseStrictlyPositiveInt(args, "wIn", &wIn);

  string constant;
  ui.parseString(args, "constant", &constant);
  mpz_class const_mpz(constant); // TODO catch exceptions here?

  return new IntConstMult(parentOp, target, wIn, const_mpz);
}

}//namespace

namespace flopoco {
	template <>
	const OperatorDescription<IntConstMult> op_descriptor<IntConstMult> {
	    "IntConstMult", // name
	    "An operator for building constant multipliers. It basically creates the instances of the highly optimized variants using shift and add or KCM", // description, string
	    "ConstMultDiv", // category, from the list defined in
	    // UserInterface.cpp
	    "",
	    "wIn(int): Wordsize of pag inputs;"
      "constant(int): constant to multiply with",
	    ""};
}//namespace


