#ifndef Xilinx_LUT5_H
#define Xilinx_LUT5_H

#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_Primitive.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"

namespace flopoco {
    class Xilinx_LUT5_base : public Xilinx_Primitive {
      public:
        Xilinx_LUT5_base(Operator *parentOp, Target *target);
        ~Xilinx_LUT5_base() {};
        void base_init(string init="");

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);
    };


    class Xilinx_LUT5 : public Xilinx_LUT5_base {
      public:
        Xilinx_LUT5(Operator *parentOp, Target *target, string init="");
        ~Xilinx_LUT5() {};

        static void registerFactory();
    };

    class Xilinx_LUT5_L : public Xilinx_LUT5_base {
      public:
        Xilinx_LUT5_L(Operator *parentOp, Target *target, string init="");
        ~Xilinx_LUT5_L() {};
    };

    class Xilinx_LUT5_D : public Xilinx_LUT5_base {
      public:
        Xilinx_LUT5_D(Operator *parentOp, Target *target, string init="");
        ~Xilinx_LUT5_D() {};
    };

    class Xilinx_CFGLUT5 : public Xilinx_LUT5_base {
      public:
        Xilinx_CFGLUT5(Operator *parentOp, Target *target, string init="");
        ~Xilinx_CFGLUT5() {};
    };

}//namespace

#endif
