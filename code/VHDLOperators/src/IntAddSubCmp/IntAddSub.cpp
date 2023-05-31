#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"
#include "flopoco/IntAddSubCmp/IntAddSub.hpp"
#include "flopoco/PrimitiveComponents/Primitive.hpp"

using namespace std;
namespace flopoco
{
  IntAddSub::IntAddSub(Operator *parentOp, Target *target, const uint32_t &wIn, const uint32_t &flags) : Operator(parentOp, target), flags_(flags)
  {
    setShared();
    setCopyrightString("Marco Kleinlein, Martin Kumm");
    this->useNumericStd();
    //disablePipelining(); //does not change anything
    srcFileName = "IntAddSub";
    ostringstream name;
    name << "IntAddSub_w" << wIn << "_" << printFlags();
    setNameWithFreqAndUID(name.str());

    addInput("X", wIn);
    addInput("Y", wIn);
    if (hasFlags(TERNARY))
      addInput("Z", wIn);

    if (hasFlags(CONF_LEFT))
      addInput("iL_c", 1, false);
    if (hasFlags(CONF_RIGHT))
      addInput("iR_c", 1, false);
    if (hasFlags(TERNARY) && hasFlags(CONF_MID))
      addInput("iM_c", 1, false);

    addOutput("sum_o", wIn);

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

  void IntAddSub::buildCommon(Target *target, const uint32_t &wIn)
  {
    const uint16_t c_count = (hasFlags(CONF_LEFT) ? 1 : 0) + (hasFlags(CONF_RIGHT) ? 1 : 0) + (hasFlags(TERNARY & CONF_MID) ? 1 : 0);
    if (c_count > 0)
    {
      vhdl << declare("CONF", c_count) << " <= ";
      if (hasFlags(CONF_LEFT))
      {
        vhdl << "iL_c";
      }
      if (hasFlags(TERNARY) and hasFlags(CONF_MID))
      {
        vhdl << "& iM_c";
      }
      if (hasFlags(CONF_RIGHT))
      {
        vhdl << "& iR_c";
      }
      else if (hasFlags(TERNARY) and hasFlags(CONF_MID))
      {
        vhdl << "iM_c";
        if (hasFlags(CONF_RIGHT))
        {
          vhdl << "& iM_c";
        }
      }
      else if (hasFlags(CONF_RIGHT))
      {
        vhdl << "& iM_c";
      }
      vhdl << " & (others =>'0')";
      vhdl << ";" << std::endl;

      vhdl << declare("XSE",wIn) << " <= ";
      uint16_t mask = 1 << (c_count - 1);
      for (int i = 0; i < (1 << c_count); ++i)
      {
        vhdl << "std_logic_vector(" << (i & (mask) ? "-" : "") << "signed(X)) when CONF=\"";
        for (int j = c_count - 1; j >= 0; --j)
          vhdl << (i & (1 << j) ? "1" : "0");
          vhdl << "\" else ";
      }
      vhdl << "(others=>'X');" << std::endl;
      /*
      vhdl << "case CONF is" << std::endl;
      declare("XSE", wIn); //!!!
      for (int i = 0; i < (1 << c_count); ++i)
      {
        vhdl << "\t" << "when \"";
        for (int j = c_count - 1; j >= 0; --j)
          vhdl << (i & (1 << j) ? "1" : "0");
        vhdl << "\"\t=> std_logic_vector(";  //!!!
//        vhdl << "\"\t=> ";  //!!!
        uint16_t mask = 1 << (c_count - 1);
        if (hasFlags(CONF_LEFT))
        {
          vhdl << (i & (mask) ? "-" : "+") << "unsigned(X)"; //!!!
//          vhdl << "XSE <= X"; //!!!
          mask >>= 1;
          if (hasFlags(TERNARY & CONF_MID))
          {
            vhdl << (i & mask ? "-" : "+") << "unsigned(Z)";
            mask >>= 1;
          }
          if (hasFlags(CONF_RIGHT))
          {
            vhdl << (i & mask ? "-" : "+") << "unsigned(Y)";
          }
        }
        else if (hasFlags(TERNARY & CONF_MID))
        {
          vhdl << "unsigned(X)";
          vhdl << (i & mask ? "-" : "+") << "unsigned(Z)";
          mask >>= 1;
          if (hasFlags(CONF_RIGHT))
          {
            vhdl << (i & mask ? "-" : "+") << "unsigned(Y)";
          }
        }
        else if (hasFlags(CONF_RIGHT))
        {
          vhdl << (i & mask ? "-" : "+") << "unsigned(Y)";
        }
        vhdl << ");" << std::endl; //!!!
//        vhdl << ";" << std::endl; //!!!
      }
      vhdl << "\t" << "when others => sum_o <= (others=>'X');" << std::endl;
      vhdl << "end case;" << std::endl;
      */
    }
    else
    {
      vhdl << "\tsum_o <= std_logic_vector(" << (hasFlags(SUB_LEFT) ? "-" : "");
      vhdl << "signed(X)";
      if (hasFlags(TERNARY))
      {
        vhdl << (hasFlags(SUB_MID) ? "-" : "+") << "signed(Z)";
      }
      vhdl << (hasFlags(SUB_RIGHT) ? "-" : "+") << "signed(Y));" << endl;
    }
  }

  string IntAddSub::getInputName(const uint32_t &index, const bool &c_input) const
  {
    switch (index)
    {
      case 0:
        return "iL" + std::string(c_input ? "_c" : "");
      case 1:
        return std::string(hasFlags(TERNARY | CONF_MID) ? "iM" : "iR") + std::string(c_input ? "_c" : "");
      case 2:
        return std::string(hasFlags(TERNARY | CONF_MID) ? "iR" + std::string(c_input ? "_c" : "") : "");
      default:
        return "";
    }
  }

  string IntAddSub::getOutputName() const
  {
    return "sum_o";
  }

  bool IntAddSub::hasFlags(const uint32_t &flag) const
  {
    return flags_ & flag;
  }

  const uint32_t IntAddSub::getInputCount() const
  {
    uint32_t c = (hasFlags(TERNARY) ? 3 : 2);
    if (hasFlags(CONF_LEFT)) c++;
    if (hasFlags(TERNARY) && hasFlags(CONF_MID)) c++;
    if (hasFlags(CONF_RIGHT)) c++;
    return c;
  }

  string IntAddSub::printFlags() const
  {
    std::stringstream o;
    o << (hasFlags(SUB_LEFT) ? "s" : "");
    o << (hasFlags(CONF_LEFT) ? "c" : "");
    o << "L";

    if (hasFlags(TERNARY))
    {
      o << (hasFlags(SUB_MID) ? "s" : "");
      o << (hasFlags(CONF_MID) ? "c" : "");
      o << "M";
    }

    o << (hasFlags(SUB_RIGHT) ? "s" : "");
    o << (hasFlags(CONF_RIGHT) ? "c" : "");
    o << "R";
    return o.str();
  }


  void IntAddSub::emulate(TestCase *tc)
  {

  }


  void IntAddSub::buildStandardTestCases(TestCaseList *tcl)
  {
    // please fill me with regression tests or corner case tests!
  }

  OperatorPtr IntAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui)
  {
    int wIn;
    ui.parseStrictlyPositiveInt(args, "wIn", &wIn, false);

    bool isTernary;
    bool xNegative;
    bool yNegative;
    bool zNegative;
    bool xConfigurable;
    bool yConfigurable;
    bool zConfigurable;
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

    uint32_t flags = 0;
    if(isTernary)
    {
      flags |= IntAddSub::TERNARY;
    }
    if(xNegative)
    {
      flags |= IntAddSub::SUB_LEFT;
    }
    if(yNegative)
    {
      flags |= IntAddSub::SUB_RIGHT;
    }
    if(zNegative)
    {
      flags |= IntAddSub::SUB_MID;
    }
    if(xConfigurable)
    {
      flags |= IntAddSub::CONF_LEFT;
    }
    if(yConfigurable)
    {
      flags |= IntAddSub::CONF_RIGHT;
    }
    if(zConfigurable)
    {
      flags |= IntAddSub::CONF_MID;
    }
    return new IntAddSub(parentOp, target, wIn, flags);
  }

  template<>
  const OperatorDescription<IntAddSub> op_descriptor<IntAddSub> {
    "IntAddSub", // name
    "Generic Integer adder/subtractor that supports addition and subtraction of up to three inputs (ternary adder) as well as runtime configuration of the signs of operation but no fancy pipelining like IntAdder.",
    "BasicInteger", // category
    "",
    "wIn(int): input size in bits;\
     isTernary(bool)=false: set to true if you want a ternary (3-input adder);\
     xNegative(bool)=false: set to true if X (first) input should be subtracted;\
     yNegative(bool)=false: set to true if Y (second) input should be subtracted;\
     zNegative(bool)=false: set to true if Z (third) input should be subtracted, only valid when isTernary=true;\
     xConfigurable(bool)=false: set to true if X input should be configurable with its sign (an extra input is added to decide add/subtract operation);\
     yConfigurable(bool)=false: set to true if Y input should be configurable with its sign (an extra input is added to decide add/subtract operation);\
     zConfigurable(bool)=false: set to true if Z input should be configurable with its sign (an extra input is added to decide add/subtract operation), only valid when isTernary=true;",
    ""};

}//namespace
