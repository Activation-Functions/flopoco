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
#include "Xilinx_LOOKAHEAD8.hpp"

using namespace std;
namespace flopoco {

    Xilinx_LOOKAHEAD8::Xilinx_LOOKAHEAD8(Operator *parentOp, Target *target ) : Xilinx_Primitive( parentOp, target ) {
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

        addOutput( "coutb" );
        addOutput( "coutd" );
        addOutput( "coutf" );
        addOutput( "couth" );
    }

    void Xilinx_LOOKAHEAD8::connectVectorPorts(Operator *parentOp,
      std::string signame_cyx, std::string signame_prop,
      std::string signame_out, int rlow_in, int rlow_out
    ) {

      // map inputs
      for (char i = 0; i < 8; i++) {
        std::stringstream port_cyx;
        std::stringstream port_prop;
        std::stringstream port_gex;

        port_cyx  <<   "cy" << std::string(1, 'a' + i);
        port_prop << "prop" << std::string(1, 'a' + i);
        port_gex  <<   "ge" << std::string(1, 'a' + i);

        parentOp->inPortMap( port_cyx.str(),  signame_cyx + of(i + rlow_in));
        parentOp->inPortMap(port_prop.str(), signame_prop + of(i + rlow_in));
        parentOp->inPortMapCst( port_gex.str(), "'0'");
      }

      // map outputs
      for (char i = 0; i < 8; i = i + 2) {
        std::stringstream port_out;

        port_out << "cout" << std::string(1, 'b' + i);

        parentOp->outPortMap(port_out.str(),
          signame_out + of((i/2) + rlow_out)
        );
      }
    }

}//namespace
