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
    setCopyrightString("Marco Kleinlein");
    this->useNumericStd();
    srcFileName = "IntAddSub";
    ostringstream name;
    name << "IntAddSub_w" << wIn << "_" << printFlags();
    setNameWithFreqAndUID(name.str());

    addInput("iL", wIn);
    addInput("iR", wIn);
    if (hasFlags(TERNARY))
      addInput("iM", wIn);

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
        if (hasFlags(TERNARY) and hasFlags(CONF_MID))
        {
          vhdl << "& iM_c";
        }
        if (hasFlags(CONF_RIGHT))
        {
          vhdl << "& iR_c";
        }
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
      vhdl << std::endl;
      vhdl << "case CONF is" << std::endl;
      for (int i = 0; i < (1 << c_count); ++i)
      {
        vhdl << "\t" << "when \"";
        for (int j = c_count - 1; j >= 0; --j)
          vhdl << (i & (1 << j) ? "1" : "0");
        vhdl << "\"\t=> std_logic_vector(";
        uint16_t mask = 1 << (c_count - 1);
        if (hasFlags(CONF_LEFT))
        {
          vhdl << (i & (mask) ? "-" : "+") << "unsigned(iL)";
          mask >>= 1;
          if (hasFlags(TERNARY & CONF_MID))
          {
            vhdl << (i & mask ? "-" : "+") << "unsigned(iM)";
            mask >>= 1;
          }
          if (hasFlags(CONF_RIGHT))
          {
            vhdl << (i & mask ? "-" : "+") << "unsigned(iR)";
          }
        }
        else if (hasFlags(TERNARY & CONF_MID))
        {
          vhdl << "unsigned(iL)";
          vhdl << (i & mask ? "-" : "+") << "unsigned(iM)";
          mask >>= 1;
          if (hasFlags(CONF_RIGHT))
          {
            vhdl << (i & mask ? "-" : "+") << "unsigned(iR)";
          }
        }
        else if (hasFlags(CONF_RIGHT))
        {
          vhdl << (i & mask ? "-" : "+") << "unsigned(iR)";
        }
        vhdl << ");" << std::endl;
      }
      vhdl << "\t" << "when others => sum_o <= (others=>'X');" << std::endl;
      vhdl << "end case;" << std::endl;
    }
    else
    {
      vhdl << "\tsum_o <= std_logic_vector(" << (hasFlags(SUB_LEFT) ? "-" : "");
      vhdl << "signed(iL)";
      if (hasFlags(TERNARY))
      {
        vhdl << (hasFlags(SUB_MID) ? "-" : "+") << "signed(iM)";
      }
      vhdl << (hasFlags(SUB_RIGHT) ? "-" : "+") << " signed(iR));" << endl;
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

  OperatorPtr IntAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
    int wIn;
    ui.parseStrictlyPositiveInt(args, "wIn", &wIn, false);
    const uint32_t flags=0;
    return new IntAddSub(parentOp, target, wIn, flags);
  }

  template <>
  const OperatorDescription<IntAddSub> op_descriptor<IntAddSub> {
    "IntAddSub", // name
    "Generic Integer adder/subtractor that supports addition and subtraction of up to three inputs (ternary adder) as well as runtime configuration of the signs of operation but no fancy pipelining like IntAdder.",
    "BasicInteger", // category
    "",
    "wIn(int): input size in bits;\
					  arch(int)=-1: -1 for automatic, 0 for classical, 1 for alternative, 2 for short latency; \
					  optObjective(int)=2: 0 to optimize for logic, 1 to optimize for register, 2 to optimize for slice/ALM count; \
					  SRL(bool)=true: optimize for shift registers",
    ""};

}//namespace
