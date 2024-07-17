// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <sys/types.h>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "flopoco/PrimitiveComponents/Xilinx/XilinxTernaryAddSubSlice.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"

using namespace std;
namespace flopoco {

    XilinxTernaryAddSubSlice::XilinxTernaryAddSubSlice(Operator *parentOp, Target *target, const uint &wIn , const bool &is_initial , const std::string &lut_content ) : Operator(parentOp, target ) {
        setCopyrightString( "Marco Kleinlein" );


        setNameWithFreqAndUID( "Xilinx_TernaryAdd_2State_slice_s" + std::to_string( wIn ) + ( is_initial ? "_init" : "" ) );
        srcFileName = "XilinxTernaryAddSubSlice";
        //addToGlobalOpList( this );
        setCombinatorial();
        setShared();
        addInput( "sel_in" );
        addInput( "x_in", wIn );
        addInput( "y_in", wIn );
        addInput( "z_in", wIn );
        addInput( "bbus_in", wIn );
        addInput( "carry_in" );
        addOutput( "carry_out" );
        addOutput( "bbus_out", wIn );
        addOutput( "sum_out", wIn );

//#define TESTSHARED
#ifndef TESTSHARED
        declare( "lut_o6", wIn );
        declare( "cc_di", 4 );
        declare( "cc_s", 4 );
        declare( "cc_o", 4 );
        declare( "cc_co", 4 );
        declare( "bbus_int", wIn );

        vhdl << tab << "bbus_out <= bbus_int;" << endl;

        addConstant( "fillup_width", "integer", join( "4 - ", wIn ) );

        if( wIn <= 0 || wIn > 4 ) {
            throw std::runtime_error( "A slice with " + std::to_string( wIn ) + " bits is not possible." );
        }

        if( wIn == 4 ) {
            vhdl << tab << tab << "cc_di <= bbus_in;" << endl;
            vhdl << tab << tab << "cc_s	 <= lut_o6;" << endl;
        } else {
            vhdl << tab << tab << "cc_di	<= (fillup_width downto 1 => '0') & bbus_in;" << endl;
            vhdl << tab << tab << "cc_s     <= (fillup_width downto 1 => '0') & lut_o6;" << endl;
        }

        for( uint i = 0; i < wIn; ++i  ) {
			Xilinx_LUT6_2 *lut_bit_i = new Xilinx_LUT6_2( this,target );
            lut_bit_i->setGeneric( "init", lut_content, 64 );
            inPortMap( "i0", "z_in" + of( i ) );
            inPortMap( "i1", "y_in" + of( i ) );
            inPortMap( "i2", "x_in" + of( i ) );
            inPortMap( "i3", "sel_in" );
            inPortMap( "i4", "bbus_in" + of( i ) );
            inPortMapCst( "i5", "'1'" );
            outPortMap( "o5", "bbus_int" + of( i ));
            outPortMap( "o6", "lut_o6" + of( i ));
            vhdl << lut_bit_i->primitiveInstance( join( "lut_bit_", i ) );
        }

        if( is_initial ) {
			Xilinx_CARRY4 *init_cc = new Xilinx_CARRY4( this,target );
            outPortMap( "co", "cc_co");
            outPortMap( "o", "cc_o");
            inPortMap( "cyinit", "carry_in" );
            inPortMapCst( "ci", "'0'" );
            inPortMap( "di", "cc_di" );
            inPortMap( "s", "cc_s" );
            vhdl << init_cc->primitiveInstance( "init_cc" );
        } else {
			Xilinx_CARRY4 *further_cc = new Xilinx_CARRY4( this,target );
            outPortMap( "co", "cc_co");
            outPortMap( "o", "cc_o");
            inPortMapCst( "cyinit", "'0'" );
            inPortMap( "ci", "carry_in" );
            inPortMap( "di", "cc_di" );
            inPortMap( "s", "cc_s" );
            vhdl << further_cc->primitiveInstance( "further_cc" );
        }

        vhdl << tab << "carry_out	<= cc_co" << of( wIn - 1 ) << ";" << std::endl;
        vhdl << tab << "sum_out <= cc_o" << range( wIn-1, 0 ) << ";" << std::endl;
#else
//        vhdl << tab << declare(1.2345E-9 ,"tmp", wIn) << "<= x_in" << ";" << endl; //assign to some tmp signal and adding some delay
//        vhdl << tab << "sum_out <= tmp" << ";" << endl;

/*
			Xilinx_LUT6_2 *lut_bit_i = new Xilinx_LUT6_2( this,target );
            lut_bit_i->setGeneric( "init", lut_content, 64 );
            inPortMap( "i0", "z_in" + of( i ) );
            inPortMap( "i1", "y_in" + of( i ) );
            inPortMap( "i2", "x_in" + of( i ) );
            inPortMap( "i3", "sel_in" );
            inPortMap( "i4", "bbus_in" + of( i ) );
            inPortMapCst( "i5", "'1'" );
            outPortMap( "o5", "bbus_int" + of( i ));
            outPortMap( "o6", "lut_o6" + of( i ));
            vhdl << lut_bit_i->primitiveInstance( join( "lut_bit_", i ) );
*/
      int i=0;

      declare( "lut_o6", wIn );
      declare( "bbus_int", wIn );

      Xilinx_LUT6_2 *lut_bit_i = new Xilinx_LUT6_2( this,target );
      lut_bit_i->setGeneric( "init", lut_content, 64 );

      vhdl << tab << declare("i0") << " <= z_in" << of(i) << ";" << endl;
      vhdl << tab << declare("i1") << " <= y_in" << of(i) << ";" << endl;
      vhdl << tab << declare("i2") << " <= x_in" << of(i) << ";" << endl;
      vhdl << tab << declare("i3") << " <= sel_in;" << endl;
      vhdl << tab << declare("i4") << " <= bbus_in" << of(i) << ";" << endl;
      vhdl << tab << declare("i5") << " <= '1';" << endl;

      std::stringstream inportmap;
      inportmap << "i0 => i0,";
      inportmap << "i1 => i1,";
      inportmap << "i2 => i2,";
      inportmap << "i3 => i3,";
      inportmap << "i4 => i4,";
      inportmap << "i5 => i5";

      std::stringstream outportmap;
      outportmap << "o5 => o5,";
      outportmap << "o6 => o6";

      newSharedInstance(lut_bit_i , "lut_bit_" + to_string(i), inportmap.str(), outportmap.str());

      vhdl << tab << "bbus_int" << of((i)) << " <= o5;" << endl;
      vhdl << tab << "lut_o6" << of((i)) << " <= o6;" << endl;

      vhdl << tab << "sum_out <= lut_o6" << ";" << endl;

#endif
	};

}//namespace
