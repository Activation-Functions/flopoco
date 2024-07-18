#include "flopoco/ConstMult/IntConstMult.hpp"
#include "flopoco/InterfacedOperator.hpp"

using namespace std;

namespace flopoco{



IntConstMult::IntConstMult(OperatorPtr parentOp, Target* target, int wIn, string constants, string method, int wOut, bool isSigned) : Operator(parentOp, target), target(target), wIn(wIn), constants(constants), method(method), wOut(wOut), isSigned(isSigned)
{
  srcFileName="IntConstMult";

  ostringstream name;
  name << "IntConstMult_" << wIn;
  setNameWithFreqAndUID(name.str());

  //convert constants string
  vector<string> rows = explode(constants, ';');

  if(rows.size() == 1) //SCM or MCM problem?
  {
    vector<string> row = explode(rows[0],',');
    noOfInputs = row.size();
    if(noOfInputs == 1) //SCM problem?
    {
      constMultClass = SCM;
      REPORT(LogLevel::DETAIL, "Problem is a single constants multiplication (SCM) problem");

      noOfOutputs=1;
      mpz_class const_mpz;

      coeffs.resize(1);
      coeffs[0].resize(1);
      coeffs[0][0] = mpz_class(row[0]);
      implementSCM();
      REPORT(LogLevel::DETAIL, "Constant is " << row[0]);
    }
    else //MCM problem
    {
      constMultClass = MCM;
      REPORT(LogLevel::DETAIL, "Problem is a multiple constants multiplication (MCM) problem");

      noOfOutputs = rows.size();
      coeffs.resize(noOfOutputs);

      for(int i=0; i < noOfOutputs; i++)
      {
        coeffs[i].resize(1);
        coeffs[i][0] = mpz_class(row[i]);
      }


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
    constMultClass = CMM;
    REPORT(LogLevel::DETAIL, "Problem is a constants matrix multiplication (CMM) problem");

    if(is_log_lvl_enabled(LogLevel::DEBUG))
    {
      REPORT(LogLevel::DETAIL, "Matrix is:");
      for(auto r : rows)
      {
        vector<string> row = explode(r,',');
        noOfInputs = row.size();
        for(auto e : row)
        {
          cerr << e << " ";
        }
        cerr << endl;
      }
    }
    THROWERROR("Sorry, CMM not implemented yet!");
  }

//  mpz_class const_mpz(constants); // TODO catch exceptions here?
//  const_mpz = mpz_class("123"); // TODO catch exceptions here?



}

void IntConstMult::implementSCM()
{
  mpz_class const_mpz = coeffs[0][0];
  if(wOut == -1)
  {
    if(const_mpz > 0)
    {
      wOut = sizeInBits(const_mpz * ((mpz_class(1)<<wIn)-1));
//      wCoeff = sizeInBits(const_mpz-1);
    }
    else
    {
//      wCoeff = sizeInBits(const_mpz);
      wOut = sizeInBits(abs(const_mpz) * ((mpz_class(1)<<wIn)));
    }
    REPORT(LogLevel::DETAIL, "No output word size wOut was given, determining word size to be " << wOut << " bits");
  }
  else
  {
    THROWERROR("sorry, truncated operators are currently not supported")
  }

  int wCoeff = wOut - wIn;

  addInput("X", wIn);
  addOutput("R", wOut);

  declare("R_tmp",wOut);

  if(method == "auto")
  {
    REPORT(LogLevel::DETAIL, "Method 'auto' was given (default), determining best method from constants and target");

    if(target->useTargetOpt() && target->hasFastLogicTernaryAdders() && (wCoeff <= 22))
    {
      REPORT(LogLevel::DETAIL, "As target supports ternary adders and word size is < 22 bit, we will use them (method=minAddTermary)");
      method = "minAddTermary";
    }
    else if(wCoeff <= 19)
    {
      REPORT(LogLevel::DETAIL, "As target does not support ternary adders and word size is < 19 bit, we will not use them (method=minAdd)");
      method = "minAdd";
    }
    else if(wCoeff <= 63)
    {
      REPORT(LogLevel::DETAIL, "As word size is <32 bit, we will use RPAG (method=ShiftAddRPAG)");
      method = "ShiftAddRPAG";
    }
    else
    {
      REPORT(LogLevel::DETAIL, "As coefficient word size is larger than 32 bits, we will use ShiftAddPlain (method=ShiftAddPlain)");
      method = "ShiftAddPlain";
    }

  }


  if(method=="minAdd")
  {
    newInstance("IntConstMultShiftAddOpt",
              "IntConstMultShiftAddOpt",
              "wIn=" + std::to_string(wIn) + " constant=" + constants + " signed=" + to_string(isSigned),
              "X0=>X",
              "R"+ factorToString(coeffs[0],MCM)+"=>R_tmp");

  }
  else if(method=="minAddTernary")
  {
    newInstance("IntConstMultShiftAddOptTernary",
              "IntConstMultShiftAddOptTernary",
              "wIn=" + std::to_string(wIn) + " constant=" + constants + " signed=" + to_string(isSigned),
              "X0=>X",
              "R"+ factorToString(coeffs[0],MCM)+"=>R_tmp");

  }
  else if(method=="ShiftAddPlain")
  {
    if(isSigned)
      THROWERROR("Method " << method << " does not support signed operation");

    //The old FloPoCo operator, may be removed soon
    newInstance("IntConstMultShiftAddPlain",
              "IntConstMultShiftAddPlain",
              "wIn=" + std::to_string(wIn) + " constant=" + constants,
              "X=>X",
              "R=>R_tmp");

  }
  else if(method=="ShiftAddRPAG")
  {
    newInstance("IntConstMultShiftAddRPAG",
              "IntConstMultShiftAddRPAG",
              "wIn=" + std::to_string(wIn) + " constants=" + constants + " signed=" + to_string(isSigned),
              "X0=>X",
              "R"+ factorToString(coeffs[0],MCM)+"=>R_tmp");
  }
  else
  {
    THROWERROR("Method " << method << " not supported for SCM problems.");
  }


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


string IntConstMult::factorToString(std::vector<mpz_class> &coeff, constMultClassType constMultClass)
{
  if(constMultClass == SCM)
  {
    return "";
  }
  else
  {
    stringstream signalName;
    for(int i=0 ; i < noOfInputs; i++)
    {
      signalName << "_c";
      if(coeff[i] < 0 )
      {
        signalName << "m";
      }
      signalName << abs( coeff[i] );
    }
    return signalName.str();
  }
}

	TestList IntConstMult::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;

    std::list<std::string> constantsToTest;  // inner vector = weights for each input, outer vector = different outputs

    constantsToTest.push_back("1");
    constantsToTest.push_back("7");
    constantsToTest.push_back("73");

    std::list<std::string> methodsToTest;  // inner vector = weights for each input, outer vector = different outputs
//    methodsToTest.push_back("auto");
    methodsToTest.push_back("minAdd");
    methodsToTest.push_back("minAddTernary");
    methodsToTest.push_back("ShiftAddPlain");
    methodsToTest.push_back("ShiftAddRPAG");

    if(testLevel == TestLevel::QUICK)
    {
      for(auto method : methodsToTest)
      {
        for(auto c : constantsToTest)
        {
          if(method == "ShiftAddPlain" && c=="1") continue; //ShiftAddPlain does not support multiplication by 1!
          paramList.push_back(make_pair("method", method));
          paramList.push_back(make_pair("wIn", "10"));
          paramList.push_back(make_pair("constant", c));
          paramList.push_back(make_pair("signed", "false"));
          testStateList.push_back(paramList);
          paramList.clear();
        }
        if(method == "ShiftAddPlain") continue; //ShiftAddPlain does not support signed!
        for(auto c : constantsToTest)
        {

          paramList.push_back(make_pair("method", method));
          paramList.push_back(make_pair("wIn", "10"));
          paramList.push_back(make_pair("constant", c));
          paramList.push_back(make_pair("signed", "true"));
          testStateList.push_back(paramList);
          paramList.clear();
        }
      }
    }
    else
    {
      for(int wIn=8; wIn<=100; wIn+=16) // test various input widths
      {
        for(auto c : constantsToTest)
        {
          paramList.push_back(make_pair("wIn", "10"));
          paramList.push_back(make_pair("constant", c));
          paramList.push_back(make_pair("signed", "true"));
          testStateList.push_back(paramList);
          paramList.clear();
        }
        for(auto c : constantsToTest)
        {
          paramList.push_back(make_pair("wIn", "10"));
          paramList.push_back(make_pair("constant", c));
          paramList.push_back(make_pair("signed", "false"));
          testStateList.push_back(paramList);
          paramList.clear();
        }
      }
    }

		return testStateList;
	}



void IntConstMult::emulate(TestCase * tc)
	{
		vector<mpz_class> inputVector(noOfInputs);

    mpz_class msbp1 = (mpz_class(1) << (wIn));
    mpz_class msb = (mpz_class(1) << (wIn - 1));

    //get inputs:
		for(int i=0 ; i < noOfInputs ; i++)
		{
		  if(constMultClass == SCM)
      {
  			inputVector[i] = tc->getInputValue("X");
      }
      else
      {
  			inputVector[i] = tc->getInputValue("X" + to_string(i));
      }

      if(isSigned)
      {
        if (inputVector[i] >= msb)
          inputVector[i] = inputVector[i] - msbp1;
      }
    }

		vector<mpz_class> outputVector(noOfOutputs);

		for(int o=0; o < noOfOutputs; o++)
    {
      for(int i=0 ; i < noOfInputs ; i++)
      {
        outputVector[o] += inputVector[i] * coeffs[o][i];
      }
      string outputName = "R" + factorToString(coeffs[o], constMultClass);
      tc->addExpectedOutput(outputName, outputVector[o], isSigned);
    }



	}

OperatorPtr flopoco::IntConstMult::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
{
  int wIn;
  ui.parseStrictlyPositiveInt(args, "wIn", &wIn);

  int wOut;
  ui.parseInt(args, "wOut", &wOut);

  string constants;
  ui.parseString(args, "constant", &constants);

  string method;
  ui.parseString(args, "method", &method);

  bool isSigned;
  ui.parseBoolean(args, "signed", &isSigned);

  return new IntConstMult(parentOp, target, wIn, constants, method, wOut, isSigned);
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
      "constant(string): constant(s) to multiply with, can be a single constant, multiple constants separated by semicolon or constant matrices, where rows are separated by comma (like in Matlab);"
      "wOut(int)=-1: Output word size (-1 means full precision according to the constant and wIn);"
      "signed(bool)=true: signedness of input and output;"
      "method(string)=auto: desired method. Can be 'KCM', 'minAdd', 'minAddTernary', 'ShiftAddRPAG', 'ShiftAddPlain' or 'auto' (let FloPoCo decide which operator performs best on the given target)",
	    ""};
}//namespace


