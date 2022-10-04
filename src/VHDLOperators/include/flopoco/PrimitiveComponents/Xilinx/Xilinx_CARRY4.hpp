#ifndef Xilinx_CARRY4_H
#define Xilinx_CARRY4_H

#include "flopoco/InterfacedOperator.hpp"
#include "Xilinx_Primitive.hpp"

namespace flopoco
{

  // new operator class declaration
  class Xilinx_CARRY4 : public Xilinx_Primitive
  {
  public:

    // constructor, defined there with two parameters (default value 0 for each)
    Xilinx_CARRY4(Operator *parentOp, Target *target);

    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

  };
}//namespace

#endif
