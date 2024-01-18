#include "flopoco/ConstMult/IntConstMult.hpp"
#include "flopoco/InterfacedOperator.hpp"

using namespace std;

namespace flopoco{



IntConstMult::IntConstMult(OperatorPtr parentOp, Target* target, int wIn, string constant, string method) : Operator(parentOp, target), wIn(wIn), constant(constant)
{
  srcFileName="IntConstMult";

  ostringstream name;
  name << "IntConstMult_" << wIn;
  setNameWithFreqAndUID(name.str());

  //convert constant string

  cerr << "!!! constant = " << constant << endl;

  vector<string> rows = explode(constant,';');

  if(rows.size() == 1) //SCM or MCM problem?
  {
    vector<string> row = explode(rows[0],',');
    if(row.size() == 1) //SCM problem?
    {
      REPORT(LogLevel::DETAIL, "Problem is a single constant multiplication (SCM) problem");

      mpz_class const_mpz;

      const_mpz = mpz_class(row[0]);
      implementSCM(const_mpz);
      REPORT(LogLevel::DETAIL, "Constant is " << row[0]);
    }
    else //MCM problem
    {
      REPORT(LogLevel::DETAIL, "Problem is a multiple constant multiplication (MCM) problem");
      REPORT(LogLevel::DETAIL, "Constants are:");
      if(is_log_lvl_enabled(LogLevel::DEBUG))
      {
        for(auto e : row)
        {
          cerr << e << " ";
        }
        cerr << endl;
      }

      THROWERROR("Sorry, MCM not implemented yet!");
    }

  }
  else //CMM problem
  {
    REPORT(LogLevel::DETAIL, "Problem is a constant matrix multiplication (CMM) problem");

    if(is_log_lvl_enabled(LogLevel::DEBUG))
    {
      REPORT(LogLevel::DETAIL, "Matrix is:");
      for(auto r : rows)
      {
        vector<string> row = explode(r,',');
        for(auto e : row)
        {
          cerr << e << " ";
        }
        cerr << endl;
      }
    }
    THROWERROR("Sorry, CMM not implemented yet!");
  }

//  mpz_class const_mpz(constant); // TODO catch exceptions here?
//  const_mpz = mpz_class("123"); // TODO catch exceptions here?

  if(method == "auto")
  {
    REPORT(LogLevel::DETAIL, "No method was given (default), determining best method from constant and target");
    if(target->hasFastLogicTernaryAdders())
    {
    }
    else
    {
    }
  }



}

void IntConstMult::implementSCM(mpz_class const_mpz)
{
  int wOut = intlog2(const_mpz * ((mpz_class(1)<<wIn)-1));

  addInput("X", wIn);
  addOutput("R", wOut);

  declare("R_tmp",wOut);
  newInstance("IntConstMultShiftAddPlain",
            "IntConstMultShiftAddPlain",
            "wIn=" + std::to_string(wIn) + " constant=" + constant,
            "X=>X",
            "R=>R_tmp");

  vhdl << tab << "R <= R_tmp;" << endl;
}


std::vector<std::string> IntConstMult::explode(std::string & s, char delim)
{
  std::vector<std::string> result;
  std::istringstream iss(s);

  for (std::string token; std::getline(iss, token, delim); )
  {
    result.push_back(std::move(token));
  }

  return result;
}


/*
void flopoco::IntConstMult::emulate(TestCase *tc){
  mpz_class X = tc->getInputValue("X");
  mpz_class R = X * const_mpz;
  tc->addExpectedOutput("R", R);
}
*/

OperatorPtr flopoco::IntConstMult::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
{
  int wIn;

  ui.parseStrictlyPositiveInt(args, "wIn", &wIn);

  string constant;
  ui.parseString(args, "constant", &constant);

  string method;
  ui.parseString(args, "method", &method);

  return new IntConstMult(parentOp, target, wIn, constant, method);
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
      "constants(string): constant(s) to multiply with, can be a single constant, comma separated multiple constants or constant matrices;"
      "method(string)=auto: desired method. Can be 'KCM', 'ShiftAdd', 'ShiftAddRPAG' or 'auto' (let FloPoCo decide which operator performs best)",
	    ""};
}//namespace


