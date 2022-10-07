// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"

using namespace std;
namespace flopoco
{
  XilinxLUT::XilinxLUT(Operator *parentOp, Target *target, int inport_cnt) : Xilinx_Primitive(parentOp, target)
  {
    srcFileName = "Xilinx_LUT6";

    for (int i = 0; i < inport_cnt; i++)
      addInput(join("i", i));
  }

  Xilinx_LUT6::Xilinx_LUT6(Operator *parentOp, Target *target, string init) : XilinxLUT(parentOp, target)
  {
    setName("LUT6");
    addOutput("o");
    if(!init.empty())
      setGeneric("init", init, 64);
    //vhdl << "o <= i0" << endl;
  }

  Xilinx_LUT6_2::Xilinx_LUT6_2(Operator *parentOp, Target *target, string init) : XilinxLUT(parentOp, target)
  {
    setName("LUT6_2");
    addOutput("o5");
    addOutput("o6");
    if(!init.empty())
      setGeneric("init", init, 64);

    /*
    vhdl << "--dummy assignment for schedule(), dirty hack!!" << endl;
    vhdl << declare(1E-9, "o5_i") << " <= i0 xor i1 xor i2 xor i3 xor i4 xor i5;" << endl;
    vhdl << declare(1E-9,"o6_i") << " <= i0 xor i1 xor i2 xor i3 xor i4 xor i5;" << endl;
    vhdl << "o5 <= o5_i;" << endl;
    vhdl << "o6 <= o6_i;" << endl;
     */
  }

  Xilinx_LUT6_L::Xilinx_LUT6_L(Operator *parentOp, Target *target, string init) : XilinxLUT(parentOp, target)
  {
    setName("LUT6_L");
    addOutput("lo");
    if(!init.empty())
      setGeneric("init", init, 64);
  }

  Xilinx_LUT6_D::Xilinx_LUT6_D(Operator *parentOp, Target *target, string init) : XilinxLUT(parentOp, target)
  {
    setName("LUT6_D");
    addOutput("o");
    addOutput("lo");
    if(!init.empty())
      setGeneric("init", init, 64);
  }

  Xilinx_LUT6_CY::Xilinx_LUT6_CY(Operator *parentOp, Target *target, string init) : XilinxLUT(parentOp, target, 5)
  {
    setName( "LUT6CY" );
    addOutput( "o51" );
    addOutput( "o52" );
    addOutput( "prop" );
    if(!init.empty())
      setGeneric("init", init, 64);
  }

  OperatorPtr XilinxLUT::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
  {
    if (target->getVendor() != "Xilinx")
      throw std::runtime_error("Can't build xilinx primitive on non xilinx target");

    string variant;
    ui.parseString(args, "variant", &variant);

    string init;
    ui.parseString(args, "init", &init);

    if(variant == "LUT6")
      return new Xilinx_LUT6(parentOp, target, init);
    else if(variant == "LUT6_2")
      return new Xilinx_LUT6_2(parentOp, target, init);
    else if(variant == "LUT6_L")
      return new Xilinx_LUT6_L(parentOp, target, init);
    else if(variant == "LUT6_D")
      return new Xilinx_LUT6_D(parentOp, target, init);
    else if(variant == "LUT6_CY")
      return new Xilinx_LUT6_CY(parentOp, target, init);
    else
      throw std::runtime_error("Unknown variant: " + variant);
  }

  template <>
  const OperatorDescription<XilinxLUT> op_descriptor<XilinxLUT> {
      "XilinxLUT",				     // name
      "Provides variants of Xilinx LUT primitives.", // description, string
      "Primitives", // category, from the list defined in UserInterface.cpp
      "",
      "variant(string): The LUT variant (LUT6, LUT6_2, etc.);\
       init(string): The LUT content;",
      ""};

}//namespace
