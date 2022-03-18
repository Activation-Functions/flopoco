// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "Xilinx_TernaryAdd_2State_slice.hpp"
#include "Xilinx_CARRY4.hpp"
#include "Xilinx_LUT6.hpp"

using namespace std;
namespace flopoco
{

  Xilinx_TernaryAdd_2State_slice::Xilinx_TernaryAdd_2State_slice(Operator *parentOp, Target *target, const uint &wIn, const bool &is_initial, const std::string &lut_content) : Operator (parentOp, target)
  {
    setCopyrightString("Marco Kleinlein, Martin Kumm");

    setNameWithFreqAndUID("Xilinx_TernaryAdd_2State_slice_s" + std::to_string(wIn) + (is_initial ? "_init" : ""));
    srcFileName = "Xilinx_TernaryAdd_2State_slice";
    //addToGlobalOpList( this );
    setCombinatorial();
    setShared(); //has to be set, otherwise
    addInput("sel_in");
    addInput("x_in", wIn);
    addInput("y_in", wIn);
    addInput("z_in", wIn);
    addInput("bbus_in", wIn);
    addInput("carry_in");
    addOutput("carry_out");
    addOutput("bbus_out", wIn);
    addOutput("sum_out", wIn);
    declare("lut_o6", wIn);
    declare("cc_di", 4);
    declare("cc_s", 4);
    declare("cc_o", 4);
    declare("cc_co", 4);
    addConstant("fillup_width", "integer", join("4 - ", wIn));

    if (wIn <= 0 || wIn > 4)
    {
      throw std::runtime_error("A slice with " + std::to_string(wIn) + " bits is not possible.");
    }

    if (wIn == 4)
    {
      vhdl << tab << tab << "cc_di <= bbus_in;" << endl;
      vhdl << tab << tab << "cc_s	 <= lut_o6;" << endl;
    }
    else
    {
      vhdl << tab << tab << "cc_di	<= (fillup_width downto 1 => '0') & bbus_in;" << endl;
      vhdl << tab << tab << "cc_s     <= (fillup_width downto 1 => '0') & lut_o6;" << endl;
    }

    declare("bbus_out_int", wIn);

    for (uint i = 0; i < wIn; ++i)
    {
      newInstance("XilinxLUT",
                  join("lut_bit_", i),
                  "variant=LUT6_2 init=" + lut_content,
                  "i0 => z_in" + of(i) + ",i1 => y_in" + of(i) + ",i2 => x_in" + of(i) + ",i3 => sel_in,i4 => bbus_in" + of(i),
                  "o5 => bbus_out_int" + of(i) + ", o6 => lut_o6" + of(i),
                  "i5=>'1'");

/*
      Xilinx_LUT6_2 *lut_bit_i = new Xilinx_LUT6_2(this, target);
      lut_bit_i->setGeneric("init", lut_content, 64);
      inPortMap("i0", "z_in" + of(i));
      inPortMap("i1", "y_in" + of(i));
      inPortMap("i2", "x_in" + of(i));
      inPortMap("i3", "sel_in");
      inPortMap("i4", "bbus_in" + of(i));
      inPortMapCst("i5", "'1'");
      outPortMap("o5", "bbus_out_int" + of(i));
      outPortMap("o6", "lut_o6" + of(i));

      vhdl << lut_bit_i->primitiveInstance(join("lut_bit_", i));
*/
    }
    vhdl << "bbus_out <= bbus_out_int;" << endl;

    if (is_initial)
    {
      newInstance("XilinxCARRY4",
                  "init_cc",
                  "",
                  "cyinit => carry_in,di => cc_di,s => cc_s",
                  "co => cc_co,o => cc_o",
                  "ci=>'0'");
/*
      Xilinx_CARRY4 *init_cc = new Xilinx_CARRY4(this, target);
      outPortMap("co", "cc_co");
      outPortMap("o", "cc_o");
      inPortMap("cyinit", "carry_in");
      inPortMapCst("ci", "'0'");
      inPortMap("di", "cc_di");
      inPortMap("s", "cc_s");
      vhdl << init_cc->primitiveInstance("init_cc");
*/
    }
    else
    {
      newInstance("XilinxCARRY4",
                  "further_cc",
                  "",
                  "ci => carry_in,di => cc_di,s => cc_s",
                  "co => cc_co,o => cc_o",
                  "cyinit=>'0'");

/*
      Xilinx_CARRY4 *further_cc = new Xilinx_CARRY4(this, target);
      outPortMap("co", "cc_co");
      outPortMap("o", "cc_o");
      inPortMapCst("cyinit", "'0'");
      inPortMap("ci", "carry_in");
      inPortMap("di", "cc_di");
      inPortMap("s", "cc_s");
      vhdl << further_cc->primitiveInstance("further_cc");

*/
    }

    vhdl << tab << "carry_out	<= cc_co" << of(wIn - 1) << ";" << std::endl;
    vhdl << tab << "sum_out <= cc_o" << range(wIn - 1, 0) << ";" << std::endl;

  };

  OperatorPtr Xilinx_TernaryAdd_2State_slice::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
  {
    if (target->getVendor() != "Xilinx")
      throw std::runtime_error("Can't build xilinx primitive on non xilinx target");

    int wIn;
    UserInterface::parseInt(args, "wIn", &wIn);

    bool is_initial;
    UserInterface::parseBoolean(args, "is_initial", &is_initial);

    string lut_content;
    UserInterface::parseString(args, "lut_content", &lut_content);

    return new Xilinx_TernaryAdd_2State_slice(parentOp, target, wIn, is_initial, lut_content);
  }

  void Xilinx_TernaryAdd_2State_slice::registerFactory()
  {
    UserInterface::add("Xilinx_TernaryAdd_2State_slice", // name
                       "Implements one slice of a ternary adder.", // description, string
                       "Primitives", // category, from the list defined in UserInterface.cpp
                       "",
                       "wIn(int)=4: input word size;\
                                  is_initial(bool)=false: Is first slice?;\
                                  lut_content(string)=69699696e8e8e8e8: content of LUTs?;",
                       "",
                       Xilinx_TernaryAdd_2State_slice::parseArguments
    );
  }
  //4, const bool &is_initial = false, const std::string &lut_content = "x\"69699696e8e8e8e8\""
}//namespace
