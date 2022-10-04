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
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"

using namespace std;
namespace flopoco
{
  Xilinx_CARRY4::Xilinx_CARRY4(Operator *parentOp, Target *target) : Xilinx_Primitive(parentOp, target)
  {
    setName("CARRY4");
    srcFileName = "Xilinx_CARRY4";
    addInput("ci");
    addInput("cyinit");
    addInput("di", 4);
    addInput("s", 4);
    addOutput("co", 4);
    addOutput("o", 4);

    /*
    vhdl << "--dummy assignment for schedule(), dirty hack!!" << endl;
    vhdl << declare(1E-9, "o_i",4) << " <= di xor s;" << endl;
    vhdl << declare(1E-9,"co_i",4) << " <= di xor s;" << endl;
    vhdl << "co <= co_i;" << endl;
    vhdl << "o <= o_i;" << endl;
  */
  }

  OperatorPtr Xilinx_CARRY4::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
  {
    if (target->getVendor() != "Xilinx")
      throw std::runtime_error("Can't build xilinx primitive on non xilinx target");

    return new Xilinx_CARRY4(parentOp, target);
  }

  template <>
  OperatorFactory op_factory<Xilinx_CARRY4>(){return factoryBuilder<Xilinx_CARRY4>({
      "XilinxCARRY4",			       // name
      "Provides the Xilinx CARRY4 primitive.", // description, string
      "Primitives", // category, from the list defined in UserInterface.cpp
      "",
      "",
      ""});}
}//namespace
