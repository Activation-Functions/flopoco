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

    cerr << "!!! fixed_signs=" << fixed_signs << endl;
		if( fixed_signs != 0 ) {
			build_with_fixed_sign( target, wIn, fixed_signs );
		} else {
			build_normal( target, wIn, false);
		}
	}

	Xilinx_GenericAddSub::Xilinx_GenericAddSub(Operator* parentOp, Target *target, int wIn, bool dss ) : Operator( parentOp, target ),wIn_(wIn),signs_(-1),dss_(dss) {
		setCopyrightString( "Marco Kleinlein, Martin Kumm" );
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
			build_normal( target, wIn, true);
		}
	}
	
	void Xilinx_GenericAddSub::build_normal( Target *target, int wIn, bool configurable ) {
    cerr << "!!! Xilinx_GenericAddSub::build_normal" << endl;
		addInput( "X", wIn );
		addInput( "Y", wIn );
    if(configurable)
    {
      addInput( "neg_X" );
      addInput( "neg_Y" );
    }
		addOutput( "R", wIn );
		addOutput( "Cout" );
		const int effective_ws = wIn + 1;
		const int ws_remain = ( effective_ws ) % 4;
		const int num_full_slices = floor( ( effective_ws - ws_remain ) / 4 );
		declare( "carry", num_full_slices + 1 );
		declare( "sum_t", effective_ws );
		declare( "x_int", effective_ws );
		declare( "y_int", effective_ws );
		declare( "neg_x_int");
		declare( "neg_y_int");
		vhdl << tab << "x_int" << range( effective_ws - 1, 1 ) << " <= X;" << std::endl;
		vhdl << tab << "y_int" << range( effective_ws - 1, 1 ) << " <= Y;" << std::endl;
    if(configurable)
    {
      vhdl << tab << "neg_x_int <= neg_X;" << std::endl;
      vhdl << tab << "neg_y_int <= neg_Y;" << std::endl;
    }
    else
    {
      vhdl << tab << "neg_x_int <= '0';" << std::endl;
      vhdl << tab << "neg_y_int <= '0';" << std::endl;
    }
		int i = 0;
		for( ; i < num_full_slices; i++ ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, 4, ( i == 0 ? true : false ), false, false, this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "X", "x_int" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "Y", "y_int" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "neg_X", "neg_x_int" );
			inPortMap( slice_i, "neg_Y", "neg_y_int" );

			if( i == 0 ) {
				inPortMapCst( slice_i, "Cin", "'0'" );
			} else {
				inPortMap( slice_i, "Cin" , "carry" + of( i - 1 ) );
			}

			outPortMap( slice_i, "Cout", "carry" + of( i ));
			outPortMap( slice_i, "R", "sum_t" + range( 4 * i + 3, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		if( ws_remain > 0 ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, ws_remain, ( i == 0 ? true : false ), false, false , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "X", "x_int" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "Y", "y_int" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "neg_X", "neg_x_int" );
			inPortMap( slice_i, "neg_Y", "neg_y_int" );

			if( i == 0 ) {
				inPortMapCst( slice_i, "Cin", "'0'" );
			} else {
				inPortMap( slice_i, "Cin" , "carry" + of( i - 1 ) );
			}

			outPortMap( slice_i, "Cout", "carry" + of( i ));
			outPortMap( slice_i, "R", "sum_t" + range( effective_ws - 1, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		vhdl << tab << "R <= sum_t" << range( effective_ws - 1, 1 ) << ";" << std::endl;
		vhdl << tab << "Cout <= carry" << of( i ) << ";" << std::endl;
	}

	void Xilinx_GenericAddSub::build_with_dss( Target *target, int wIn ) {
		addInput( "X", wIn );
		addInput( "Y", wIn );
		addInput( "neg_X" );
		addInput( "neg_Y" );
		addOutput( "R", wIn );
		addOutput( "Cout" );
		const int effective_ws = wIn;
		const int ws_remain = ( effective_ws ) % 4;
		const int num_full_slices = ( effective_ws - ws_remain ) / 4;
		declare( "carry", num_full_slices + 1 );
		declare( "sum_t", effective_ws );
		declare( "x_int", effective_ws );
		declare( "y_int", effective_ws );
		declare( "neg_x_int" );
		declare( "neg_y_int" );
		declare( "bbus", wIn + 1 );
		vhdl << tab << "x_int" << range( effective_ws - 1, 0 ) << " <= X;" << std::endl;
		vhdl << tab << "y_int" << range( effective_ws - 1, 0 ) << " <= Y;" << std::endl;
		vhdl << tab << "neg_x_int <= neg_X;" << std::endl;
		vhdl << tab << "neg_y_int <= neg_Y;" << std::endl;
		vhdl << tab << "bbus" << of( 0 ) << " <= '0';" << std::endl;
		int i = 0;

		for( ; i < num_full_slices; i++ ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, 4, ( i == 0 ? true : false ), false, true , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "X", "x_int" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "Y", "y_int" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "neg_X", "neg_x_int" );
			inPortMap( slice_i, "neg_Y", "neg_y_int" );
			inPortMap( slice_i, "bbus_in", "bbus" + range( 4 * i + 3, 4 * i ) );
			outPortMap( slice_i, "bbus_out" , "bbus" + range( 4 * i + 4, 4 * i + 1 ));

			if( i == 0 ) {
				inPortMapCst( slice_i, "Cin", "'0'" );
			} else {
				inPortMap( slice_i, "Cin" , "carry" + of( i-1 ) );
			}

			outPortMap( slice_i, "Cout", "carry" + of( i ));
			outPortMap( slice_i, "R", "sum_t" + range( 4 * i + 3, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		if( ws_remain > 0 ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, ws_remain, ( i == 0 ? true : false ), false, true , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "X", "x_int" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "Y", "y_int" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "neg_X", "neg_x_int" );
			inPortMap( slice_i, "neg_Y", "neg_y_int" );
			inPortMap( slice_i, "bbus_in", "bbus" + range( effective_ws - 1, 4 * i ) );
			outPortMap( slice_i, "bbus_out" , "bbus" + range( effective_ws, 4 * i + 1 ));

			if( i == 0 ) {
				inPortMapCst( slice_i, "Cin", "'0'" );
			} else {
				inPortMap( slice_i, "Cin" , "carry" + of( i-1 ) );
			}

      outPortMap( slice_i, "Cout", "carry" + of( i ));
			outPortMap( slice_i, "R", "sum_t" + range( effective_ws - 1, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		vhdl << tab << "R <= sum_t" << range( effective_ws - 1, 0 ) << ";" << std::endl;
    vhdl << tab << "Cout <= carry" << of( i ) << ";" << std::endl;
	}

	void Xilinx_GenericAddSub::build_with_fixed_sign( Target *target, int wIn, int fixed_signs ) {
		addInput( "X", wIn );
		addInput( "Y", wIn );
		addOutput( "R", wIn );
		addOutput( "Cout" );
		const int effective_ws = wIn;
		const int ws_remain = ( effective_ws ) % 4;
		const int num_full_slices = ( effective_ws - ws_remain ) / 4;
		declare( "carry", num_full_slices + 1 );
		declare( "sum_t", effective_ws );
		declare( "x_int", effective_ws );
		declare( "y_int", effective_ws );
		vhdl << tab << "x_int" << range( effective_ws - 1 , 0 ) << " <= X;" << std::endl;
		vhdl << tab << "y_int" << range( effective_ws - 1, 0 ) << " <= Y;" << std::endl;
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
			inPortMap( slice_i, "X", "x_int" + range( 4 * i + 3, 4 * i ) );
			inPortMap( slice_i, "Y", "y_int" + range( 4 * i + 3, 4 * i ) );
			inPortMapCst( slice_i, "neg_X", neg_x );
			inPortMapCst( slice_i, "neg_Y", neg_y );

			if( i == 0 ) {
				if( fixed_signs > 0 ) {
					inPortMapCst( slice_i, "Cin", "'1'" );
				} else {
					inPortMapCst( slice_i, "Cin", "'0'" );
				}
			} else {
				inPortMap( slice_i, "Cin" , "carry" + of(i-1));
			}

			outPortMap( slice_i, "Cout", "carry" + of( i ));
			outPortMap( slice_i, "R", "sum_t" + range( 4 * i + 3, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		if( ws_remain > 0 ) {
			stringstream slice_name;
			slice_name << "slice_" << i;
			Xilinx_GenericAddSub_slice *slice_i = new Xilinx_GenericAddSub_slice( this, target, ws_remain, ( i == 0 ? true : false ), true, false , this->getName() );
			addSubComponent( slice_i );
			inPortMap( slice_i, "X", "x_int" + range( effective_ws - 1, 4 * i ) );
			inPortMap( slice_i, "Y", "y_int" + range( effective_ws - 1, 4 * i ) );
			inPortMapCst( slice_i, "neg_X", neg_x );
			inPortMapCst( slice_i, "neg_Y", neg_y );

			if( i == 0 ) {
				if( fixed_signs > 0 ) {
					inPortMapCst( slice_i, "Cin", "'1'" );
				} else {
					inPortMapCst( slice_i, "Cin", "'0'" );
				}
			} else {
				inPortMap( slice_i, "Cin" , "carry" + of(i-1));
			}

      outPortMap( slice_i, "Cout", "carry" + of(i));
			outPortMap( slice_i, "R", "sum_t" + range( effective_ws - 1, 4 * i ));
			vhdl << instance( slice_i, slice_name.str() );
		}

		vhdl << tab << "R <= sum_t" << range( effective_ws - 1, 0 ) << ";" << std::endl;
    vhdl << tab << "Cout <= carry" << of( i ) << ";" << std::endl;
	}

	Xilinx_GenericAddSub::~Xilinx_GenericAddSub() {}

	OperatorPtr Xilinx_GenericAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui){
		if( target->getVendor() != "Xilinx" )
			throw std::runtime_error( "Can't build xilinx primitive on non xilinx target" );

		int wIn;
    ui.parseInt(args,"wIn",&wIn );

    bool xNegative;
    ui.parseBoolean(args,"xNegative",&xNegative);
    bool yNegative;
    ui.parseBoolean(args,"yNegative",&yNegative);
    bool configurable;
    ui.parseBoolean(args,"configurable",&configurable);
    bool allowBothInputsNegative;
    ui.parseBoolean(args,"allowBothInputsNegative",&allowBothInputsNegative);

    int fixed_signs=0;
    if(xNegative)
      fixed_signs |= 1; //x input

    if(yNegative)
      fixed_signs |= 2; //y input

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
      "xNegative(bool)=false: set to true if x (first) input should be subtracted, only works when yNegative=false;"
      "yNegative(bool)=false: set to true if y (second) input should be subtracted, only works when xNegative=false;"
      "configurable(bool)=false: set to true if signs of input should be configurable at runtime (an extra input is added to decide on add/subtract operation);"
      "allowBothInputsNegative(bool)=false: Allows both inputs to be negative in the runtime configurable configuration (at the cost of a slower implementation)"
      ,
	    ""};
/*
*/


  void Xilinx_GenericAddSub::emulate(TestCase *tc)
	{
		bool neg_a=false,neg_b=false;
		mpz_class x = tc->getInputValue("X");
		mpz_class y = tc->getInputValue("Y");
		mpz_class s = 0;
		if( signs_ > 0 ){
			if( signs_ & 0x1 )
				neg_a = true;

			if( signs_ & 0x2 )
				neg_b = true;
		} else {
			mpz_class na = tc->getInputValue("neg_X");
			mpz_class nb = tc->getInputValue("neg_Y");

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

		tc->addExpectedOutput("R", s);
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
      paramList.push_back(make_pair("xNegative", "true"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("configurable", "false"));
      paramList.push_back(make_pair("allowBothInputsNegative", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "true"));
      paramList.push_back(make_pair("configurable", "false"));
      paramList.push_back(make_pair("allowBothInputsNegative", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("configurable", "true"));
      paramList.push_back(make_pair("allowBothInputsNegative", "false"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("xNegative", "false"));
      paramList.push_back(make_pair("yNegative", "false"));
      paramList.push_back(make_pair("configurable", "true"));
      paramList.push_back(make_pair("allowBothInputsNegative", "true"));
      testStateList.push_back(paramList);
      paramList.clear();
    }
    return testStateList;
  }
}//namespace


