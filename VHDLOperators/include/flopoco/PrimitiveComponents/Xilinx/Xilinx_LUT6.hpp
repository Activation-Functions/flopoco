#ifndef Xilinx_LUT6_H
#define Xilinx_LUT6_H

#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_Primitive.hpp"

#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"

namespace flopoco
{
  enum LUT6_VARIANT
  {
    LUT6_GENERAL_OUT,
    LUT6_TWO_OUT,
    LUT6_LOCAL_OUT,
    LUT6_GENERAL_AND_LOCAL_OUT
  };

  // new operator class declaration
  class XilinxLUT : public Xilinx_Primitive
  {
  public:
    XilinxLUT(Operator *parentOp, Target *target, int inport_cnt=6);

    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);
    static void registerFactory();
  };

  class Xilinx_LUT6 : public XilinxLUT
  {
  public:
    Xilinx_LUT6(Operator *parentOp, Target *target, string init="");
  };

  class Xilinx_LUT6_2 : public XilinxLUT
  {
  public:
    Xilinx_LUT6_2(Operator *parentOp, Target *target, string init="");
  };

  class Xilinx_LUT6_L : public XilinxLUT
  {
  public:
    Xilinx_LUT6_L(Operator *parentOp, Target *target, string init="");
  };

  class Xilinx_LUT6_D : public XilinxLUT
  {
  public:
    Xilinx_LUT6_D(Operator *parentOp, Target *target, string init="");
  };

  // avaible since versal
  class Xilinx_LUT6_CY : public XilinxLUT {
    public:
      	Xilinx_LUT6_CY(Operator* parentOp, Target *target, string init="");
  };
}//namespace

#endif
