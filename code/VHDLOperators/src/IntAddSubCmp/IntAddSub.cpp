#include <iostream>
#include <sstream>

#include "gmp.h"
#include <gmpxx.h>
#include "mpfr.h"
#include "flopoco/IntAddSubCmp/IntAddSub.hpp"
#include "flopoco/PrimitiveComponents/Primitive.hpp"

using namespace std;
namespace flopoco
{
  IntAddSub::IntAddSub(Operator *parentOp, Target *target, const uint32_t &wIn, const bool isSigned, const bool isTernary, const bool xNegative, const bool yNegative, const bool zNegative, const bool xConfigurable, const bool yConfigurable, const bool zConfigurable) : Operator(parentOp, target), wIn(wIn), isSigned(isSigned), isTernary(isTernary), xNegative(xNegative), yNegative(yNegative), zNegative(zNegative), xConfigurable(xConfigurable), yConfigurable(yConfigurable), zConfigurable(zConfigurable)
  {
    setShared();
    setCopyrightString("Martin Kumm");
    this->useNumericStd();
    //disablePipelining(); //does not change anything
    srcFileName = "IntAddSub";
    ostringstream name;
    name << "IntAddSub_w" << wIn << "_" << printFlags();
    setNameWithFreqAndUID(name.str());

    addInput("X", wIn);
    addInput("Y", wIn);
    if (isTernary)
      addInput("Z", wIn);

    if (xConfigurable)
      addInput("negX", 1, false);
    if (yConfigurable)
      addInput("negY", 1, false);
    if (isTernary && zConfigurable)
      addInput("negZ", 1, false);

    wOut = wIn + (isTernary ? 2 : 1);
    addOutput("R", wOut);

    if (target->useTargetOptimizations() && target->getVendor() == "Xilinx")
      buildXilinx(target, wIn);
    else if (target->useTargetOptimizations() && target->getVendor() == "Altera")
      buildAltera(target, wIn);
    else
      buildCommon(target, wIn);

  }

  void IntAddSub::buildXilinx(Target *target, const uint32_t &wIn)
  {
    REPORT(LogLevel::MESSAGE, "Xilinx junction not fully implemented, fall back to common.");
    buildCommon(target, wIn);

  }

  void IntAddSub::buildAltera(Target *target, const uint32_t &wIn)
  {
    REPORT(LogLevel::MESSAGE, "Altera junction not fully implemented, fall back to common.");
    buildCommon(target, wIn);

  }

  void IntAddSub::generateInternalInputSignal(string name, int wIn)
  {
    vhdl << tab << declare(name + "_int",wIn) << " <= ";
    if(xConfigurable)
    {
      vhdl << "std_logic_vector(-signed(" + name + ")) when neg" + name + "='1' else " + name;
    }
    else
    {
      if(xNegative)
      {
        vhdl << "std_logic_vector(-signed(" + name + "))";
      }
      else
      {
        vhdl << name;
      }
    }
    vhdl <<  ";" << endl;
  }

  void IntAddSub::buildCommon(Target *target, const uint32_t &wIn)
  {
    generateInternalInputSignal("X", wIn);
    generateInternalInputSignal("Y", wIn);
    if(isTernary)
      generateInternalInputSignal("Z", wIn);

    string se_method;
    if(isSigned)
    {
      se_method = "signed";
    }
    else
    {
      se_method = "unsigned";
    }
    vhdl << tab << "R <= std_logic_vector(resize(" << se_method << "(X_int)," << wOut << ") + resize(" << se_method << "(Y_int)," << wOut << ")";
    if(isTernary)
      vhdl << "+ resize(" << se_method << "(Z_int)," << wOut << ")";
    vhdl << ");" << endl;

  }

  string IntAddSub::getInputName(const uint32_t &index, const bool &c_input) const
  {
    switch (index)
    {
      case 0:
        return "X";
      case 1:
        return "Y";
      case 2:
        return "Z";
      default:
        return "";
    }
  }

  string IntAddSub::getOutputName() const
  {
    return "Y";
  }

  /*
  bool IntAddSub::hasFlags(const uint32_t &flag) const
  {
    return flags_ & flag;
  }
*/
  const uint32_t IntAddSub::getInputCount() const
  {
    uint32_t c = (isTernary ? 3 : 2);
    if (xConfigurable) c++;
    if (yConfigurable) c++;
    if (isTernary && zConfigurable) c++;
    return c;
  }

  string IntAddSub::printFlags() const
  {
    std::stringstream o;
    o << (xNegative ? "s" : "");
    o << (xConfigurable ? "c" : "");
    o << "X";

    o << (yNegative ? "s" : "");
    o << (yConfigurable ? "c" : "");
    o << "Y";

    if (isTernary)
    {
      o << (zNegative ? "s" : "");
      o << (zConfigurable ? "c" : "");
      o << "Z";
    }

    return o.str();
  }


  void IntAddSub::emulate(TestCase *tc)
  {
		mpz_class twoToWin = (mpz_class(1) << (wIn));
		mpz_class twoToWin_m_1 = (mpz_class(1) << (wIn - 1));
		mpz_class twoToWout = (mpz_class(1) << (wOut));

    bool nega=false,negb=false;
    mpz_class x = tc->getInputValue("X");
    mpz_class y = tc->getInputValue("Y");
    mpz_class z = 0;

    if(isTernary)
    {
      z = tc->getInputValue("Z");
    }

    if(isSigned)
    {
      if(x >= twoToWin_m_1)
      {
        x -= twoToWin; //MSB is set, so compute the two's complement by subtracting 2^wIn
      }

      if(y >= twoToWin_m_1)
      {
        y -= twoToWin; //MSB is set, so compute the two's complement by subtracting 2^wIn
      }
      if(isTernary)
      {
        if(z >= twoToWin_m_1)
        {
          z -= twoToWin; //MSB is set, so compute the two's complement by subtracting 2^wIn
        }
      }
    }

    mpz_class s = 0;

    mpz_class negX = 0;
    if(xConfigurable)
    {
      negX = tc->getInputValue("negX");
    }
    mpz_class negY = 0;
    if(yConfigurable)
    {
      negY = tc->getInputValue("negY");
    }
    mpz_class negZ = 0;
    if(isTernary && zConfigurable)
    {
      negZ = tc->getInputValue("negZ");
    }

    if( xNegative || (xConfigurable && negX==1))
      s -= x;
    else
      s += x;

    if( yNegative || (yConfigurable && negY==1))
      s -= y;
    else
      s += y;

    if(isTernary)
    {
      if( zNegative || (zConfigurable && negZ==1))
        s -= z;
      else
        s += z;
    }

    if(s < 0)
    {
      s += twoToWout;  //s is negative, so compute the two's complement by adding 2^wIn
    }
    tc->addExpectedOutput("R", s);
  }

  void IntAddSub::buildStandardTestCases(TestCaseList *tcl)
  {
    // please fill me with regression tests or corner case tests!
  }

  TestList IntAddSub::unitTest(int index)
  {
    TestList testStateList;
    vector<pair<string, string>> paramList;

    if(index==-1)
    {// Unit tests
      for(int isSigned=0; isSigned <= 1; isSigned++)
      {
        for(int isTernary=0; isTernary <= 1; isTernary++)
        {
          for(int xNegative=0; xNegative <= 1; xNegative++)
          {
            for(int yNegative=0; yNegative <= 1; yNegative++)
            {
              for(int zNegative=0; zNegative <= 1; zNegative++)
              {
                for(int xConfigurable=0; xConfigurable <= 1; xConfigurable++)
                {
                  for(int yConfigurable=0; yConfigurable <= 1; yConfigurable++)
                  {
                    for(int zConfigurable=0; zConfigurable <= 1; zConfigurable++)
                    {
                      if(!isTernary && zConfigurable) continue; //invalid parameter combination
                      if(!isTernary && zNegative) continue; //invalid parameter combination
                      paramList.push_back(make_pair("wIn", "10"));
                      paramList.push_back(make_pair("isSigned", isSigned ? "true" : "false"));
                      paramList.push_back(make_pair("isTernary", isTernary ? "true" : "false"));
                      paramList.push_back(make_pair("xNegative", xNegative ? "true" : "false"));
                      paramList.push_back(make_pair("yNegative", yNegative ? "true" : "false"));
                      paramList.push_back(make_pair("zNegative", zNegative ? "true" : "false"));
                      paramList.push_back(make_pair("xConfigurable", xConfigurable ? "true" : "false"));
                      paramList.push_back(make_pair("yConfigurable", yConfigurable ? "true" : "false"));
                      paramList.push_back(make_pair("zConfigurable",zConfigurable ? "true" : "false"));
                      testStateList.push_back(paramList);
                      paramList.clear();
                    }
                  }
                }
              }
            }
          }
        }
      }
      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("isSigned", "false"));
      paramList.push_back(make_pair("isTernary", "false"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("zNegative", "false"));
      paramList.push_back(make_pair("xConfigurable", "false"));
      paramList.push_back(make_pair("yConfigurable", "false"));
      paramList.push_back(make_pair("zConfigurable", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("isSigned", "true"));
      paramList.push_back(make_pair("isTernary", "false"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("zNegative", "false"));
      paramList.push_back(make_pair("xConfigurable", "false"));
      paramList.push_back(make_pair("yConfigurable", "false"));
      paramList.push_back(make_pair("zConfigurable", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("isSigned", "false"));
      paramList.push_back(make_pair("isTernary", "true"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("zNegative", "false"));
      paramList.push_back(make_pair("xConfigurable", "false"));
      paramList.push_back(make_pair("yConfigurable", "false"));
      paramList.push_back(make_pair("zConfigurable", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("isSigned", "true"));
      paramList.push_back(make_pair("isTernary", "true"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("zNegative", "false"));
      paramList.push_back(make_pair("xConfigurable", "false"));
      paramList.push_back(make_pair("yConfigurable", "false"));
      paramList.push_back(make_pair("zConfigurable", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

    }
    return testStateList;
  }

  OperatorPtr IntAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui)
  {
    int wIn;
    ui.parseStrictlyPositiveInt(args, "wIn", &wIn, false);

    bool isSigned;
    bool isTernary;
    bool xNegative;
    bool yNegative;
    bool zNegative;
    bool xConfigurable;
    bool yConfigurable;
    bool zConfigurable;

    ui.parseBoolean(args, "isSigned", &isSigned);
    ui.parseBoolean(args, "isTernary", &isTernary);
    ui.parseBoolean(args, "xNegative", &xNegative);
    ui.parseBoolean(args, "yNegative", &yNegative);
    ui.parseBoolean(args, "zNegative", &zNegative);
    ui.parseBoolean(args, "xConfigurable", &xConfigurable);
    ui.parseBoolean(args, "yConfigurable", &yConfigurable);
    ui.parseBoolean(args, "zConfigurable", &zConfigurable);

    if((zNegative && !isTernary) || (zConfigurable && !isTernary))
    {
      cerr << "Error: zNegative or zConfigurable can not be true when isTernary is false" << endl;
      exit(-1);
    }

    return new IntAddSub(parentOp, target, wIn, isSigned, isTernary, xNegative, yNegative, zNegative, xConfigurable, yConfigurable, zConfigurable);

  }

  template<>
  const OperatorDescription<IntAddSub> op_descriptor<IntAddSub> {
    "IntAddSub", // name
    "Generic Integer adder/subtractor that supports addition and subtraction of up to three inputs (ternary adder) as well as runtime configuration of the signs of operation but no fancy pipelining like IntAdder.",
    "BasicInteger", // category
    "",
    "wIn(int): input size in bits;\
     isSigned(bool)=false: set to true if you want a signed adder (with correct sign extension of the inputs);\
     isTernary(bool)=false: set to true if you want a ternary (3-input adder);\
     xNegative(bool)=false: set to true if X (first) input should be subtracted;\
     yNegative(bool)=false: set to true if Y (second) input should be subtracted;\
     zNegative(bool)=false: set to true if Z (third) input should be subtracted, only valid when isTernary=true;\
     xConfigurable(bool)=false: set to true if X input should be configurable with its sign (an extra input is added to decide add/subtract operation);\
     yConfigurable(bool)=false: set to true if Y input should be configurable with its sign (an extra input is added to decide add/subtract operation);\
     zConfigurable(bool)=false: set to true if Z input should be configurable with its sign (an extra input is added to decide add/subtract operation), only valid when isTernary=true;",
    ""};

}//namespace
