#ifndef Xilinx_LOOKAHEAD8_H
#define Xilinx_LOOKAHEAD8_H

#include "flopoco/InterfacedOperator.hpp"
#include "Xilinx_Primitive.hpp"

namespace flopoco {

    // avaible since versal
    class Xilinx_LOOKAHEAD8 : public Xilinx_Primitive {
      public:

        Xilinx_LOOKAHEAD8(Operator *parentOp, Target *target,
          std::string lookb, std::string lookd,
          std::string lookf, std::string lookh
        );

        ~Xilinx_LOOKAHEAD8() {};

        // as lookahead interface is not vector based (it has a named port
        // for each bit) this make it somewhat of a hassle to use it in a
        // generic codegen scenario, this convienience helper method eases
        // the pain somewhat by automatically mapping a vector based signal
        // to corresponding named ports
        static void connectVectorPorts(Operator *parentOp,
          std::string signame_cyx, std::string signame_prop,
          std::string signame_out, int rlow_in = 0, int rlow_out = 0
        );

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
        static OperatorPtr newInstanceForVectorConnections(
           Operator *parentOp, std::string instname, std::string params,
           std::string signame_cyx, std::string signame_prop,
           std::string signame_out, std::string extra_ins,
           std::string extra_ins_cst="", int rlow_in=0, int rlow_out=0
        );

    };
}//namespace

#endif
