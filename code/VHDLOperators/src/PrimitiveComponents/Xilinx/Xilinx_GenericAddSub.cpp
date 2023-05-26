// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "flopoco/UserInterface.hpp"
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_GenericAddSub.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_GenericAddSub_slice.hpp"

using namespace std;
namespace flopoco {

	Xilinx_GenericAddSub::Xilinx_GenericAddSub(Operator* parentOp, Target *target, int wIn, int fixed_signs ) : Operator( parentOp, target ),wIn_(wIn),signs_(fixed_signs),dss_(false) {
		setCopyrightString( "Marco Kleinlein, Martin Kumm" );

		srcFileName = "Xilinx_GenericAddSub";
		stringstream name;
		name << "Xilinx_GenericAddSub_" << wIn;

		if( fixed_signs != -1 ) {
			name << "_fixed_" << ( fixed_signs & 0x1 ) << ( ( fixed_signs & 0x2 ) >> 1 ) ;
		}

		setName( name.str() );
		setCombinatorial();
		addLUT( wIn + 1 );

		if( fixed_signs != -1 ) {
			build_with_fixed_sign( target, wIn, fixed_signs );
		} else {
			build_normal( target, wIn );
		}
	}

	Xilinx_GenericAddSub::Xilinx_GenericAddSub(Operator* parentOp, Target *target, int wIn, bool dss ) : Operator( parentOp, target ),wIn_(wIn),signs_(-1),dss_(dss) {
		setCopyrightString( "Marco Kleinlein" );
		srcFileName = "Xilinx_GenericAddSub";
		stringstream name;
		name << "Xilinx_GenericAddSub_" << wIn;

		if( dss ) {
			name << "_dss";
		}

		setNameWithFreqAndUID( name.str() );
		setCombinatorial();
		addLUT( wIn + 1 );

		if( dss ) {
			build_with_dss( target, wIn );
		} else {
			build_normal( target, wIn );
		}
	}
	
	void Xilinx_GenericAddSub::build_normal( Target *target, int wIn ) {
		addInput( "x_i", wIn );
		addInput( "y_i", wIn );
		addInput( "neg_x_i" );
		addInput( "neg_y_i" );
		addOutput( "sum_o", wIn );
		addOutput( "c_o" );
		const int effective_ws = wIn + 1;
		const int ws_remain = ( effective_ws ) % 4;
		const int num_full_slices = floor( ( effective_ws - ws_remain ) / 4 );
		declare( "carry", num_full_slices + 1 );
		declare( "sum_t", effective_ws );
		declare( "x", effective_ws );
		declare( "y", effective_ws );
		declare( "neg_x");
		declare( "neg_y");
		vhdl << tab << "x" << range( effective_ws - 1, 1 ) << " <= x_i;" << std::endl;
		vhdl << tab << "y" << range( effective_ws - 1, 1 ) << " <= y_i;" << std::endl;
		vhdl << tab << "neg_x <= neg_x_i;" << std::endl;
		vhdl << tab << "neg_y <= neg_y_i;" << std::endl;
		int i = 0;

		for( ; i < num_full_slices; i++ ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, 4, ( i == 0 ? true : false ), false, false, this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "x_in", "x" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "y_in", "y" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "neg_x_in", "neg_x" );
			inPortMap( slice_i, "neg_y_in", "neg_y" );

			if( i == 0 ) {
				inPortMapCst( slice_i, "carry_in", "'0'" );
			} else {
				inPortMap( slice_i, "carry_in" , "carry" + of( i - 1 ) );
			}

			outPortMap( slice_i, "carry_out", "carry" + of( i ));
			outPortMap( slice_i, "sum_out", "sum_t" + range( 4 * i + 3, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		if( ws_remain > 0 ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, ws_remain, ( i == 0 ? true : false ), false, false , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "x_in", "x" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "y_in", "y" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "neg_x_in", "neg_x" );
			inPortMap( slice_i, "neg_y_in", "neg_y" );

			if( i == 0 ) {
				inPortMapCst( slice_i, "carry_in", "'0'" );
			} else {
				inPortMap( slice_i, "carry_in" , "carry" + of( i - 1 ) );
			}

			outPortMap( slice_i, "carry_out", "carry" + of( i ));
			outPortMap( slice_i, "sum_out", "sum_t" + range( effective_ws - 1, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		vhdl << tab << "sum_o <= sum_t" << range( effective_ws - 1, 1 ) << ";" << std::endl;
		vhdl << tab << "c_o <= carry" << of( i ) << ";" << std::endl;
	}

	void Xilinx_GenericAddSub::build_with_dss( Target *target, int wIn ) {
		addInput( "x_i", wIn );
		addInput( "y_i", wIn );
		addInput( "neg_x_i" );
		addInput( "neg_y_i" );
		addOutput( "sum_o", wIn );
		addOutput( "c_o" );
		const int effective_ws = wIn;
		const int ws_remain = ( effective_ws ) % 4;
		const int num_full_slices = ( effective_ws - ws_remain ) / 4;
		declare( "carry", num_full_slices + 1 );
		declare( "sum_t", effective_ws );
		declare( "x", effective_ws );
		declare( "y", effective_ws );
		declare( "neg_x" );
		declare( "neg_y" );
		declare( "bbus", wIn + 1 );
		vhdl << tab << "x" << range( effective_ws - 1, 0 ) << " <= x_i;" << std::endl;
		vhdl << tab << "y" << range( effective_ws - 1, 0 ) << " <= y_i;" << std::endl;
		vhdl << tab << "neg_x <= neg_x_i;" << std::endl;
		vhdl << tab << "neg_y <= neg_y_i;" << std::endl;
		vhdl << tab << "bbus" << of( 0 ) << " <= '0';" << std::endl;
		int i = 0;

		for( ; i < num_full_slices; i++ ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, 4, ( i == 0 ? true : false ), false, true , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "x_in", "x" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "y_in", "y" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "neg_x_in", "neg_x" );
			inPortMap( slice_i, "neg_y_in", "neg_y" );
			inPortMap( slice_i, "bbus_in", "bbus" + range( 4 * i + 3, 4 * i ) );
			outPortMap( slice_i, "bbus_out" , "bbus" + range( 4 * i + 4, 4 * i + 1 ));

			if( i == 0 ) {
				inPortMapCst( slice_i, "carry_in", "'0'" );
			} else {
				inPortMap( slice_i, "carry_in" , "carry" + of( i-1 ) );
			}

			outPortMap( slice_i, "carry_out", "carry" + of( i ));
			outPortMap( slice_i, "sum_out", "sum_t" + range( 4 * i + 3, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		if( ws_remain > 0 ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, ws_remain, ( i == 0 ? true : false ), false, true , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "x_in", "x" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "y_in", "y" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "neg_x_in", "neg_x" );
			inPortMap( slice_i, "neg_y_in", "neg_y" );
			inPortMap( slice_i, "bbus_in", "bbus" + range( effective_ws - 1, 4 * i ) );
			outPortMap( slice_i, "bbus_out" , "bbus" + range( effective_ws, 4 * i + 1 ));

			if( i == 0 ) {
				inPortMapCst( slice_i, "carry_in", "'0'" );
			} else {
				inPortMap( slice_i, "carry_in" , "carry" + of( i-1 ) );
			}

      outPortMap( slice_i, "carry_out", "carry" + of( i ));
			outPortMap( slice_i, "sum_out", "sum_t" + range( effective_ws - 1, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		vhdl << tab << "sum_o <= sum_t" << range( effective_ws - 1, 0 ) << ";" << std::endl;
    vhdl << tab << "c_o <= carry" << of( i ) << ";" << std::endl;
	}

	void Xilinx_GenericAddSub::build_with_fixed_sign( Target *target, int wIn, int fixed_signs ) {
		addInput( "x_i", wIn );
		addInput( "y_i", wIn );
		addOutput( "sum_o", wIn );
		addOutput( "c_o" );
		const int effective_ws = wIn;
		const int ws_remain = ( effective_ws ) % 4;
		const int num_full_slices = ( effective_ws - ws_remain ) / 4;
		declare( "carry", num_full_slices + 1 );
		declare( "sum_t", effective_ws );
		declare( "x", effective_ws );
		declare( "y", effective_ws );
		vhdl << tab << "x" << range( effective_ws - 1 , 0 ) << " <= x_i;" << std::endl;
		vhdl << tab << "y" << range( effective_ws - 1, 0 ) << " <= y_i;" << std::endl;
		std::string neg_x, neg_y;

		if( fixed_signs != 3 ) {
			if( fixed_signs & 0x1 ) {
				neg_x = "'1'";
			} else {
				neg_x = "'0'";
			}

			if( fixed_signs & 0x2 ) {
				neg_y = "'1'";
			} else {
				neg_y = "'0'";
			}
		} else {
			throw "up";
		}

		int i = 0;

		//for(;(4*i+3)<=wIn;i++){
		for( ; i < num_full_slices; i++ ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, 4, ( i == 0 ? true : false ), true, false , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "x_in", "x" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "y_in", "y" + range( 4 * i + 3, 4 * i ) );
			inPortMapCst( slice_i, "neg_x_in", neg_x );
			inPortMapCst( slice_i, "neg_y_in", neg_y );

			if( i == 0 ) {
				if( fixed_signs > 0 ) {
					inPortMapCst( slice_i, "carry_in", "'1'" );
				} else {
					inPortMapCst( slice_i, "carry_in", "'0'" );
				}
			} else {
				inPortMap( slice_i, "carry_in" , "carry" + of(i-1));
			}

			outPortMap( slice_i, "carry_out", "carry" + of( i ));
			outPortMap( slice_i, "sum_out", "sum_t" + range( 4 * i + 3, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		if( ws_remain > 0 ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, ws_remain, ( i == 0 ? true : false ), true, false , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "x_in", "x" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "y_in", "y" + range( effective_ws - 1, 4 * i ) );
			inPortMapCst( slice_i, "neg_x_in", neg_x );
			inPortMapCst( slice_i, "neg_y_in", neg_y );

			if( i == 0 ) {
				if( fixed_signs > 0 ) {
					inPortMapCst( slice_i, "carry_in", "'1'" );
				} else {
					inPortMapCst( slice_i, "carry_in", "'0'" );
				}
			} else {
				inPortMap( slice_i, "carry_in" , "carry" + of(i-1));
			}

      outPortMap( slice_i, "carry_out", "carry" + of(i));
			outPortMap( slice_i, "sum_out", "sum_t" + range( effective_ws - 1, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		vhdl << tab << "sum_o <= sum_t" << range( effective_ws - 1, 0 ) << ";" << std::endl;
    vhdl << tab << "c_o <= carry" << of( i ) << ";" << std::endl;
	}

	Xilinx_GenericAddSub::~Xilinx_GenericAddSub() {}

	OperatorPtr Xilinx_GenericAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui){
		if( target->getVendor() != "Xilinx" )
			throw std::runtime_error( "Can't build xilinx primitive on non xilinx target" );

		int wIn;
    ui.parseInt(args,"wIn",&wIn );

    bool leftNegative;
    ui.parseBoolean(args,"leftNegative",&leftNegative);
    bool rightNegative;
    ui.parseBoolean(args,"rightNegative",&rightNegative);
    bool configurable;
    ui.parseBoolean(args,"configurable",&configurable);
    bool allowBothInputsNegative;
    ui.parseBoolean(args,"allowBothInputsNegative",&allowBothInputsNegative);

    int fixed_signs=0;
    if(leftNegative)
      fixed_signs |= 1; //x is left input

    if(rightNegative)
      fixed_signs |= 2; //y is right input

    if((fixed_signs & 3) == 3)
    {
      cerr << "Error: Both inputs negative at the same time is not allowed!" << endl;
      exit(-1);
    }

    if(configurable)
    {
      if(fixed_signs != 0)
      {
        cerr << "Error: No fixed signs are allowed when configurable!" << endl;
        exit(-1);
      }
      if(allowBothInputsNegative)
      {
        return new Xilinx_GenericAddSub(parentOp, target, wIn, true);
      }
      else
      {
        return new Xilinx_GenericAddSub(parentOp, target, wIn, false);
      }
    }
    else
    {
      if(allowBothInputsNegative)
      {
        cerr << "Error: allowBothInputsNegative is only allowed for a configurable Add/Sub!" << endl;
        exit(-1);
      }
      return new Xilinx_GenericAddSub(parentOp, target, wIn, fixed_signs);
    }
	}

	template <>
	const OperatorDescription<Xilinx_GenericAddSub> op_descriptor<Xilinx_GenericAddSub> {
	    "XilinxAddSub",				       // name
	    "An adder/subtractor build of xilinx primitives.", // description
	    "Primitives", // category, from the list defined in UserInterface.cpp
	    "",
	    "wIn(int): The wordsize of the adder;"
      "leftNegative(bool)=false: set to true if left (first) input should be subtracted, only works when rightNegative=false;"
      "rightNegative(bool)=false: set to true if right (second) input should be subtracted, only works when leftNegative=false;"
      "configurable(bool)=false: set to true if signs of input should be configurable at runtime (an extra input is added to decide on add/subtract operation);"
      "allowBothInputsNegative(bool)=false: Allows both inputs to be negative in the runtime configurable configuration (at the cost of a slower implementation)"
      ,
	    ""};
/*
*/


  void Xilinx_GenericAddSub::emulate(TestCase *tc)
	{
		bool neg_a=false,neg_b=false;
		mpz_class x = tc->getInputValue("x_i");
		mpz_class y = tc->getInputValue("y_i");
		mpz_class s = 0;
		if( signs_ > 0 ){
			if( signs_ & 0x1 )
				neg_a = true;

			if( signs_ & 0x2 )
				neg_b = true;
		} else {
			mpz_class na = tc->getInputValue("neg_x_i");
			mpz_class nb = tc->getInputValue("neg_y_i");

			if( na > 0 )
				neg_a = true;

			if( nb > 0 )
				neg_b = true;
		}

    if(neg_a && neg_b && !dss_)
    {
      //when dss is false, negating both inputs is not allowed and leads to result +1, this is simulated here
      s = 1;
    }
    else
    {
      if( neg_a )
        s -= x;
      else
        s += x;

      if( neg_b )
        s -= y;
      else
        s += y;

      mpz_class mask = ((1<<wIn_)-1);
      s = s & mask;
    }

		tc->addExpectedOutput("sum_o", s);
	}

  TestList Xilinx_GenericAddSub::unitTest(int index)
  {
    TestList testStateList;
    vector<pair<string, string>> paramList;

    if(index==-1)
    {// Unit tests

      //this tests all different cases (< 1 slice, > 1 slice up to 3 fully used slices:
      for(int wIn=1; wIn<=12; wIn++){
        string wInStr = to_string(wIn);
        paramList.push_back(make_pair("wIn", wInStr));
        testStateList.push_back(paramList);
        paramList.clear();
      }

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("leftNegative", "true"));
      paramList.push_back(make_pair("rightNegative", "false"));
      paramList.push_back(make_pair("configurable", "false"));
      paramList.push_back(make_pair("allowBothInputsNegative", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("leftNegative", "false"));
      paramList.push_back(make_pair("rightNegative", "true"));
      paramList.push_back(make_pair("configurable", "false"));
      paramList.push_back(make_pair("allowBothInputsNegative", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("leftNegative", "false"));
      paramList.push_back(make_pair("rightNegative", "false"));
      paramList.push_back(make_pair("configurable", "true"));
      paramList.push_back(make_pair("allowBothInputsNegative", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("leftNegative", "false"));
      paramList.push_back(make_pair("rightNegative", "false"));
      paramList.push_back(make_pair("configurable", "true"));
      paramList.push_back(make_pair("allowBothInputsNegative", "true"));
      testStateList.push_back(paramList);
      paramList.clear();
    }
    return testStateList;
  }
}//namespace


