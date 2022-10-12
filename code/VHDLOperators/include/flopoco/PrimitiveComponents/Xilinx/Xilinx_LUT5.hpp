#ifndef Xilinx_LUT5_H
#define Xilinx_LUT5_H

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_Primitive.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"

namespace flopoco {
    class Xilinx_LUT5_base : public Xilinx_Primitive {
      public:
        Xilinx_LUT5_base(Operator *parentOp, Target *target);
        void base_init(string init="");

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
    };


    class Xilinx_LUT5 : public Xilinx_LUT5_base {
      public:
        Xilinx_LUT5(Operator *parentOp, Target *target, string init="");
    };

    class Xilinx_LUT5_L : public Xilinx_LUT5_base {
      public:
        Xilinx_LUT5_L(Operator *parentOp, Target *target, string init="");
    };

    class Xilinx_LUT5_D : public Xilinx_LUT5_base {
      public:
        Xilinx_LUT5_D(Operator *parentOp, Target *target, string init="");
    };

    class Xilinx_CFGLUT5 : public Xilinx_LUT5_base {
      public:
        Xilinx_CFGLUT5(Operator *parentOp, Target *target, string init="");
    };

}//namespace

#endif
