// general c++ library for manipulating streams
#include <iostream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "flopoco/UserInterface.hpp"
#include "gmp.h"

// include the header of the Operator
#include "flopoco/PrimitiveComponents/Xilinx/XilinxTernaryAddSub.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/XilinxTernaryAddSubSlice.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"

using namespace std;
namespace flopoco
{

  XilinxTernaryAddSub::XilinxTernaryAddSub(Operator *parentOp, Target *target, const int &wIn, const short &bitmask, const short &bitmask2)
    : Operator(parentOp, target), wIn_(wIn), bitmask_(bitmask), bitmask2_(bitmask2)
  {
    setCopyrightString("Marco Kleinlein, Martin Kumm");
    if (bitmask2_ == -1)
    {
      bitmask2_ = bitmask_;
    }

    srcFileName = "XilinxTernaryAddSub";
    stringstream namestr;
    namestr << "XilinxTernaryAddSub_ws" << wIn << "_s" << (bitmask_ & 0x7);

    if (bitmask2_ != bitmask_)
    {
      namestr << "_s" << (bitmask2_ & 0x7);
    }

    setNameWithFreqAndUID(namestr.str());
    string lut_content = computeLUT();
    setCombinatorial();
    addInput("X", wIn);
    addInput("Y", wIn);
    addInput("Z", wIn);

    if (bitmask_ == bitmask2_)
    {
      vhdl << declare("sel") << " <= '0';" << std::endl;
    }
    else
    {
      addInput("sel");
    }

    addOutput("R", wIn);
    const uint num_slices = ((wIn - 1) / 4) + 1;
    declare("x_int", wIn);
    declare("y_int", wIn);
    declare("z_int", wIn);
    declare("bbus", wIn + 1);
    declare("carry_cct");
    declare("carry", num_slices);

    if (wIn <= 0)
    {
      THROWERROR("An adder with wordsize 0 is not possible.");
    }

    insertCarryInit();
    vhdl << tab << "x_int <= X;" << std::endl;
    vhdl << tab << "y_int <= Y;" << std::endl;
    vhdl << tab << "z_int <= Z;" << std::endl;

    if (num_slices == 1)
    {
      XilinxTernaryAddSubSlice *single_slice = new XilinxTernaryAddSubSlice(this, target, wIn, true, lut_content);
      addSubComponent(single_slice);
      inPortMap("x_in", "x_int");
      inPortMap("y_in", "y_int");
      inPortMap("z_in", "z_int");
      inPortMap("sel_in", "sel");
      inPortMap("bbus_in", "bbus" + range(wIn - 1, 0));
      inPortMap("carry_in", "carry_cct");
      outPortMap("bbus_out", "bbus" + range(wIn, 1));
      outPortMap("carry_out", "carry" + of(0));
      outPortMap("sum_out", "R" + range(wIn - 1, 0));
      vhdl << instance(single_slice, "slice_i");
    }

    if (num_slices > 1)
    {
      for (uint i = 0; i < num_slices; ++i)
      {
        if (i == 0)
        {  // FIRST SLICE
          XilinxTernaryAddSubSlice *first_slice = new XilinxTernaryAddSubSlice(this, target, 4, true, lut_content);

//#define OLD
#ifdef OLD
          addSubComponent(first_slice);
          inPortMap("x_in", "x_int" + range(3, 0));
          inPortMap("y_in", "y_int" + range(3, 0));
          inPortMap("z_in", "z_int" + range(3, 0));
          inPortMap("sel_in", "sel");
          inPortMap("bbus_in", "bbus" + range(3, 0));
          inPortMap("carry_in", "carry_cct");
          outPortMap("bbus_out", "bbus" + range(4, 1));
          outPortMap("carry_out", "carry" + of(0));
          outPortMap("sum_out", "R" + range(3, 0));
          vhdl << instance(first_slice, join("slice_", i)) << endl;
#else
          vhdl << tab << declare("x_in0",4) << " <= x_int" << range(3, 0) << ";" << endl;
          vhdl << tab << declare("y_in0",4) << " <= y_int" << range(3, 0) << ";" << endl;
          vhdl << tab << declare("z_in0",4) << " <= z_int" << range(3, 0) << ";" << endl;
          vhdl << tab << declare("bbus_in0",4) << " <= bbus" << range(3, 0) << ";" << endl;

      		std::stringstream inportmap;
      		std::stringstream outportmap;
          inportmap << "x_in => x_in0,";
          inportmap << "y_in => y_in0,";
          inportmap << "z_in => z_in0,";
          inportmap << "sel_in => sel,";
          inportmap << "bbus_in => bbus_in0,";
          inportmap << "carry_in => carry_cct";

          outportmap << "bbus_out => bbus_out0,";
          outportmap << "carry_out => carry0,";
          outportmap << "sum_out => sum_out0";

          vhdl << tab << "bbus" << range(4, 1) << " <= " << declare("bbus_out0",4) << ";" << endl;
          vhdl << tab << "carry" + of(0) << " <= " << declare("carry0") << ";" << endl;
          vhdl << tab << "R" << range(3, 0) << " <= " << declare("sum_out0",4) << ";" << endl;


//      		newInstance("IntSquarer", "squarerX", squarer_args, "X=>X", "R=>XX");
//				newSharedInstance(selfunctiontable , "SelFunctionTable" + to_string(i), "X=>"+seli, "Y=>"+ qi);
          newSharedInstance(first_slice , "first_slice", inportmap.str(), outportmap.str());
#endif
        }
        else if (i == (num_slices - 1))
        { // LAST SLICE
          XilinxTernaryAddSubSlice *last_slice = new XilinxTernaryAddSubSlice(this, target, wIn - (4 * i), false, lut_content);
          addSubComponent(last_slice);
          inPortMap("x_in", "x_int" + range(wIn - 1, 4 * i));
          inPortMap("y_in", "y_int" + range(wIn - 1, 4 * i));
          inPortMap("z_in", "z_int" + range(wIn - 1, 4 * i));
          inPortMap("sel_in", "sel");
          inPortMap("bbus_in", "bbus" + range(wIn - 1, 4 * i));
          inPortMap("carry_in", "carry" + of(i - 1));
          outPortMap("bbus_out", "bbus" + range(wIn, 4 * i + 1));
          outPortMap("carry_out", "carry" + of(i));
          outPortMap("sum_out", "R" + range(wIn - 1, 4 * i));
          vhdl << instance(last_slice, join("slice_", i)) << endl;
        }
        else
        {
          XilinxTernaryAddSubSlice *full_slice = new XilinxTernaryAddSubSlice(this, target, 4, false, lut_content);
          addSubComponent(full_slice);
          inPortMap("x_in", "x_int" + range((4 * i) + 3, 4 * i));
          inPortMap("y_in", "y_int" + range((4 * i) + 3, 4 * i));
          inPortMap("z_in", "z_int" + range((4 * i) + 3, 4 * i));
          inPortMap("sel_in", "sel");
          inPortMap("bbus_in", "bbus" + range((4 * i) + 3, 4 * i));
          inPortMap("carry_in", "carry" + of(i - 1));
          outPortMap("bbus_out", "bbus" + range((4 * i) + 4, 4 * i + 1));
          outPortMap("carry_out", "carry" + of(i));
          outPortMap("sum_out", "R" + range((4 * i) + 3, 4 * i));
          vhdl << instance(full_slice, join("slice_", i)) << endl;
        }
      }
    }
  };

  XilinxTernaryAddSub::~XilinxTernaryAddSub()
  {}

  string XilinxTernaryAddSub::computeLUT()
  {
    lut_op add_o5_co;
    lut_op add_o6_so;

    for (int i = 0; i < 32; ++i)
    {
      int s = 0;

      for (int j = 0; j < 3; j++)
      {
        if (i & (0x8))
        {
          if (((bitmask2_ & (1 << (2 - j))) && !(i & (1 << j))) || (!(bitmask2_ & (1 << (2 - j))) && (i & (1 << j))))
          {
            s++;
          }
        }
        else
        {
          if (((bitmask_ & (1 << (2 - j))) && !(i & (1 << j))) || (!(bitmask_ & (1 << (2 - j))) && (i & (1 << j))))
          {
            s++;
          }
        }
      }

      if (s & 0x2)
      {
        lut_op co_part;

        for (int j = 0; j < 5; j++)
        {
          if (i & (1 << j))
          {
            co_part = co_part & lut_in(j);
          }
          else
          {
            co_part = co_part & (~lut_in(j));
          }
        }

        add_o5_co = add_o5_co | co_part;
      }

      if (i & 0x10)
      {
        s++;
      }

      if (s & 0x1)
      {
        lut_op so_part;

        for (int j = 0; j < 5; j++)
        {
          if (i & (1 << j))
          {
            so_part = so_part & lut_in(j);
          }
          else
          {
            so_part = so_part & (~lut_in(j));
          }
        }

        add_o6_so = add_o6_so | so_part;
      }
    }

    lut_init lut_add3(add_o5_co, add_o6_so);
    return lut_add3.get_hex();
  }

  void XilinxTernaryAddSub::insertCarryInit()
  {
    int bitmask_ccount = 0, bitmask2_ccount = 0;

    for (int i = 0; i < 3; i++)
    {
      if (bitmask_ & (1 << i))
      {
        bitmask_ccount++;
      }

      if (bitmask2_ & (1 << i))
      {
        bitmask2_ccount++;
      }
    }

    string carry_cct, bbus;

    if (abs(bitmask_ccount - bitmask2_ccount) == 0)
    {
      switch (bitmask2_ccount)
      {
        case 0:
          carry_cct = "'0'";
          bbus = "'0'";
          break;

        case 1:
          carry_cct = "'0'";
          bbus = "'1'";
          break;

        case 2:
          carry_cct = "'1'";
          bbus = "'1'";
          break;
      }
    }
    else if (abs(bitmask_ccount - bitmask2_ccount) == 1)
    {
      if (bitmask_ccount == 1)
      {
        if (bitmask2_ccount == 2)
        {
          carry_cct = "'1'";
          bbus = "sel";
        }
        else
        {
          carry_cct = "'0'";
          bbus = "not sel";
        }
      }
      else if (bitmask_ccount == 0)
      {
        carry_cct = "'0'";
        bbus = "sel";
      }
      else
      {
        carry_cct = "'1'";
        bbus = "not sel";
      }
    }
    else if (abs(bitmask_ccount - bitmask2_ccount) == 2)
    {
      if (bitmask_ccount == 0)
      {
        carry_cct = "sel";
        bbus = "sel";
      }
      else
      {
        carry_cct = "not sel";
        bbus = "not sel";
      }
    }

    if (carry_cct.empty() || bbus.empty())
    {
      THROWERROR("No carry init found");
    }

    vhdl << "\tcarry_cct <= " << carry_cct << ";" << endl;
    vhdl << "\tbbus(0) <= " << bbus << ";" << endl;
  }

  void XilinxTernaryAddSub::computeState()
  {
    if (bitmask_ == bitmask2_)
    {
      bitmask2_ = -1;
    }

    short states[] = {bitmask_, bitmask2_};
    bool lock[] = {false, false, false};
    stringstream debug;
    int k_max = 2;

    if (bitmask2_ == -1)
    {
      k_max = 1;
    }

    mapping[0] = 0;
    mapping[1] = 1;
    mapping[2] = 2;

    if (states[0] & 0x4)
    {
      lock[2] = true;
    }

    for (int k = 0; k < k_max; k++)
    {
      for (int i = 2; i >= 0; --i)
      {
        if (states[k] & (1 << mapping[i]))
        {
          for (int j = i; j < 2; ++j)
          {
            if (!lock[j + 1])
            {
              short t = mapping[j + 1];
              mapping[j + 1] = mapping[j];
              mapping[j] = t;
            }
            else
            {
              break;
            }
          }
        }
      }

      for (int i = 2; i >= 0; --i)
      {
        if (states[k] & (1 << mapping[i]))
        {
          lock[i] = true;
        }
      }
    }

    debug << "Ternary_2State with states {" << bitmask_ << "," << bitmask2_ << "}" << endl;
    short states_postmap[] = {0, 0};

    for (int k = 0; k < k_max; k++)
    {
      for (int i = 0; i < 3; i++)
      {
        if (states[k] & (1 << mapping[i]))
        {
          states_postmap[k] |= (1 << i);
        }
      }
    }

    if (bitmask2_ == -1)
    {
      states_postmap[1] = -1;
    }
    else
    {
      if ((states_postmap[0] & 0x6) == 6 && (states_postmap[1] & 0x6) == 2)
      {
        short t = mapping[1];
        mapping[1] = mapping[2];
        mapping[2] = t;
        states_postmap[1] = (states_postmap[1] & 0x1) + 4;
      }
    }

    for (int i = 0; i < 3; i++)
    {
      debug << "map " << i << " to " << mapping[i] << endl;
    }

    debug << "postmap states {" << states_postmap[0] << "," << states_postmap[1] << "}" << endl;
    state_type = 0;

    if (states_postmap[0] == 0)
    {
      if (states_postmap[1] == 4)
      {
        state_type = 3;
      }
      else if (states_postmap[1] == 6)
      {
        state_type = 5;
      }
      else if (states_postmap[1] == -1)
      {
        state_type = 0;
      }
    }
    else if (states_postmap[0] == 4)
    {
      if (states_postmap[1] == 2)
      {
        state_type = 4;
      }
      else if (states_postmap[1] == 6)
      {
        state_type = 6;
      }
      else if (states_postmap[1] == 3)
      {
        state_type = 7;
      }
      else if (states_postmap[1] == 0)
      {
        state_type = 3;
      }
      else if (states_postmap[1] == -1)
      {
        state_type = 1;
      }
    }
    else if (states_postmap[0] == 6)
    {
      if (states_postmap[1] == 5)
      {
        state_type = 8;
      }
      else if (states_postmap[1] == 1)
      {
        state_type = 7;
      }
      else if (states_postmap[1] == 0)
      {
        state_type = 5;
      }
      else if (states_postmap[1] == 4)
      {
        state_type = 6;
      }
      else if (states_postmap[1] == -1)
      {
        state_type = 2;
      }
    }

    debug << "found type " << state_type << endl;
    //cerr << debug.str();
    REPORT(DEBUG, debug.str());

    if (bitmask_ > 0 && state_type == 0)
    {
      THROWERROR("2State type not valid");
    }

  }

  OperatorPtr XilinxTernaryAddSub::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui)
  {
    if (target->getVendor() != "Xilinx")
      throw std::runtime_error("Can't build xilinx primitive on non xilinx target");

    int wIn;
    int bitmask, bitmask2;
    ui.parseInt(args, "wIn", &wIn);
    ui.parseInt(args, "bitmask1", &bitmask);
    ui.parseInt(args, "bitmask2", &bitmask2);
    return new XilinxTernaryAddSub(parentOp, target, wIn, bitmask, bitmask2);
  }

  template<>
  const OperatorDescription<XilinxTernaryAddSub> op_descriptor<XilinxTernaryAddSub> {
    "XilinxTernaryAddSub",              // name
    "A ternary adder subtractor build of xilinx primitives.", // description,
    // string
    "Hidden", // category, from the list defined in
    // UserInterface.cpp
    "",
    "wIn(int): The wordsize of the adder; \
      bitmask1(int)=0:  First bitmask for input negation, when setting one of the three LSBs to one, the corresponding input (from X to Z) is negated, hence, bitmask1 can be {0,1,...,7}; \
      bitmask2(int)=-1: Second bitmask for configurable input negation, when different to bitmask1, a sel input selects the negation, bitmask1 is active when sel=0, bitmask2 is active when sel=1, bitmask2=-1 means same as bitmask1, so no configuration, up to two inputs can be changed by configuration at the same time;",
    ""};

  void XilinxTernaryAddSub::emulate(TestCase *tc)
  {
    mpz_class x = tc->getInputValue("X");
    mpz_class y = tc->getInputValue("Y");
    mpz_class z = tc->getInputValue("Z");
    mpz_class s = 0;
    mpz_class sel = 0;
    if (bitmask_ != bitmask2_)
    {
      sel = tc->getInputValue("sel");
    }
    short signs = 0;
    if (sel == 0)
    {
      signs = bitmask_;
    }
    else
    {
      signs = bitmask2_;
    }

    if (0x1 & signs)
      s -= x;
    else
      s += x;

    if (0x2 & signs)
      s -= y;
    else
      s += y;

    if (0x4 & signs)
      s -= z;
    else
      s += z;


    mpz_class one = 1;

    mpz_class mask = ((one << wIn_) - 1);
    s = s & mask;

    tc->addExpectedOutput("R", s);
  }

  TestList XilinxTernaryAddSub::unitTest(int testLevel)
  {
    TestList testStateList;
    vector<pair<string, string>> paramList;

    if (testLevel == TestLevel::QUICK)
    { // The quick tests
      paramList.push_back(make_pair("wIn", "10"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("bitmask1", "3"));
      testStateList.push_back(paramList);
      paramList.clear();

      paramList.push_back(make_pair("wIn", "10"));
      paramList.push_back(make_pair("bitmask1", "1"));
      paramList.push_back(make_pair("bitmask1", "2"));
      testStateList.push_back(paramList);
      paramList.clear();
    }
    else if (testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
      for (int w = 2; w <= 32; w++)
      {
        paramList.push_back(make_pair("wIn", to_string(w)));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("wIn", to_string(w)));
        paramList.push_back(make_pair("bitmask1", "3"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("wIn", to_string(w)));
        paramList.push_back(make_pair("bitmask1", "1"));
        paramList.push_back(make_pair("bitmask1", "2"));
        testStateList.push_back(paramList);
        paramList.clear();
      }
    }
    else
    {
      // finite number of random test computed out of testLevel
    }

    return testStateList;
  }

}//namespace
