// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <stdexcept>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "flopoco/UserInterface.hpp"
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LOOKAHEAD8.hpp"

using namespace std;
namespace flopoco {

    Xilinx_LOOKAHEAD8::Xilinx_LOOKAHEAD8(Operator *parentOp, Target *target,
      std::string lookb, std::string lookd,
      std::string lookf, std::string lookh
    ) : Xilinx_Primitive( parentOp, target ) {

      setName( "LOOKAHEAD8" );
      srcFileName = "Xilinx_LOOKAHEAD8";

      addInput( "cin" );

      addInput( "cya" );
      addInput( "cyb" );
      addInput( "cyc" );
      addInput( "cyd" );
      addInput( "cye" );
      addInput( "cyf" );
      addInput( "cyg" );
      addInput( "cyh" );

      //
      // note: this signals are never used (they are not even
      //   mentioned in the official docu), but still are there and
      //   must be connected (constant zero is fine), my personal
      //   guess is that they are relics of an earlier design phase,
      //   but why they are not properly removed, I dont know
      //
      addInput( "gea" );
      addInput( "geb" );
      addInput( "gec" );
      addInput( "ged" );
      addInput( "gee" );
      addInput( "gef" );
      addInput( "geg" );
      addInput( "geh" );

      addInput( "propa" );
      addInput( "propb" );
      addInput( "propc" );
      addInput( "propd" );
      addInput( "prope" );
      addInput( "propf" );
      addInput( "propg" );
      addInput( "proph" );

      addOutput( "cerrb" );
      addOutput( "cerrd" );
      addOutput( "cerrf" );
      addOutput( "cerrh" );

      setGeneric("LOOKB", "\"" + lookb + "\"", 5);
      setGeneric("LOOKD", "\"" + lookd + "\"", 5);
      setGeneric("LOOKF", "\"" + lookf + "\"", 5);
      setGeneric("LOOKH", "\"" + lookh + "\"", 5);
    }


    OperatorPtr Xilinx_LOOKAHEAD8::parseArguments(
      OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui
    ) {

      if (target->getVendor() != "Xilinx") {
        throw std::runtime_error(
           "Can't build xilinx primitive on non xilinx target"
        );
      }

      string lookb;
      ui.parseString(args, "lookb", &lookb);

      string lookd;
      ui.parseString(args, "lookd", &lookd);

      string lookf;
      ui.parseString(args, "lookf", &lookf);

      string lookh;
      ui.parseString(args, "lookh", &lookh);

      return new Xilinx_LOOKAHEAD8(parentOp, target,
        lookb, lookd, lookf, lookh
      );
    }

    template <>
    const OperatorDescription<Xilinx_LOOKAHEAD8> op_descriptor<Xilinx_LOOKAHEAD8> {
  "XilinxLOOKAHEAD8", // name
	"Provides the Xilinx LOOKAHEAD8 primitive introduced by Versal.", // description,
									  // string
	"Hidden", // category, from the list defined in UserInterface.cpp.
		// The (HIDDEN) is parsed to hide the operator from the default list.
	"",
	"lookb(string)=FALSE: lookahead generic \"bool\" param (Values: \"TRUE\" | \"FALSE\");\
          lookd(string)=FALSE: lookahead generic \"bool\" param (Values: \"TRUE\" | \"FALSE\");\
          lookf(string)=FALSE: lookahead generic \"bool\" param (Values: \"TRUE\" | \"FALSE\");\
          lookh(string)=FALSE: lookahead generic \"bool\" param (Values: \"TRUE\" | \"FALSE\")",
	""};

    OperatorPtr Xilinx_LOOKAHEAD8::newInstanceForVectorConnections(
      Operator *parentOp, std::string instname, std::string params,
      std::string signame_cyx, std::string signame_prop,
      std::string signame_out, std::string extra_ins,
      std::string extra_ins_cst, int rlow_in, int rlow_out
    ) {

      // map inputs
      std::stringstream inp;
      std::stringstream inp_cst;

      bool first = true;

      for (char i = 0; i < 8; i++) {
        if (first) {
          first = false;
        } else {
              inp << ",";
          inp_cst << ",";
        }

             inp <<   "cy" << std::string(1, 'a' + i) << "=>"
                 << signame_cyx + of(i + rlow_in);

             inp << ",prop" << std::string(1, 'a' + i) << "=>"
                 << signame_prop + of(i + rlow_in);

        inp_cst  <<   "ge" << std::string(1, 'a' + i) << "=>'0'";
      }

      // map outputs
      std::stringstream outp;

      first = true;

      for (char i = 0; i < 8; i = i + 2) {
        if (first) {
          first = false;
        } else {
          outp << ",";
        }

        outp << "cerr" << std::string(1, 'b' + i) << "=>"
             << signame_out + of((i/2) + rlow_out);
      }

      //std::cerr << "create new lookahead instance" << std::endl;
      //std::cerr << "final inport mapping:\n  " << inp.str() + extra_ins << std::endl;
      //std::cerr << "final constant inport mapping:\n  " << inp_cst.str() + extra_ins_cst << std::endl;
      //std::cerr << "final outport mapping:\n  " << outp.str() << std::endl;

      return parentOp->newInstance("XilinxLOOKAHEAD8", instname, params,
         inp.str() + extra_ins, outp.str(), inp_cst.str() + extra_ins_cst
      );
    }

}//namespace
