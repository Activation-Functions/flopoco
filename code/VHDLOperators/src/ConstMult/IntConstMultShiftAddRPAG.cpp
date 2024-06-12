/*
  Integer constant multiplication using the RPAG algorithm:

  Kumm, M., Zipf, P., Faust, M. & Chang, C.-H. Pipelined Adder Graph Optimization for High Speed Multiple Constant Multiplication. in 49–52 (IEEE International Symposium on Circuits and Systems (ISCAS), 2012). doi:10.1109/iscas.2012.6272072.

  Author : Martin Kumm (martin.kumm@cs.hs-fulda.de)

  All rights reserved.

*/
#include "flopoco/UserInterface.hpp"
#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)

#include <iostream>
#include <sstream>
#include <vector>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include "flopoco/utils.hpp"
#include "flopoco/Operator.hpp"

#include "flopoco/ConstMult/IntConstMultShiftAddRPAG.hpp"

#include "pagsuite/log2_64.h"
#include "pagsuite/fundamental.h"

#include <algorithm>
#include <cassert>

using namespace std;
using namespace PAGSuite;

namespace flopoco{

    IntConstMultShiftAddRPAG::IntConstMultShiftAddRPAG(Operator* parentOp, Target* target, int wIn, list<mpz_class> &coeffList, bool isSigned, int epsilon)  : IntConstMultShiftAdd(parentOp, target, wIn, "", isSigned, epsilon)
    {
		srcFileName="IntConstMultShiftAddRPAG";

		set<int_t> target_set;
		int depth=-1;
		for(auto c : coeffList)
		{
			int_t coeff = mpz_get_ui(c.get_mpz_t());
			target_set.insert(coeff);
			depth = max(depth,log2c_64(nonzeros(coeff)));
		}

		REPORT(LogLevel::DETAIL, "depth=" << depth);

		PAGSuite::rpag *rpag = new PAGSuite::rpag(); //default is RPAG with 2 input adders

		for(int_t t : target_set)
			rpag->target_set->insert(t);

		if(depth > 3)
		{
			REPORT(LogLevel::DEBUG, "depth is 4 or more, limit search limit to 1");
			rpag->search_limit = 1;
		}
		if(depth > 4)
		{
			REPORT(LogLevel::DEBUG, "depth is 5 or more, limit MSD permutation limit");
			rpag->msd_digit_permutation_limit = 1000;
		}
		PAGSuite::global_verbose = static_cast<int>(get_log_lvl())-1; //set rpag to one less than verbose of FloPoCo

		PAGSuite::cost_model_t cost_model = PAGSuite::LL_FPGA;// with default value
		rpag->input_wordsize = wIn;
		rpag->set_cost_model(cost_model);
		rpag->optimize();

		vector<set<int_t>> pipeline_set = rpag->get_best_pipeline_set();

		list<realization_row<int_t> > pipelined_adder_graph;
		pipeline_set_to_adder_graph(pipeline_set, pipelined_adder_graph, true, rpag->get_c_max());
		append_targets_to_adder_graph(pipeline_set, pipelined_adder_graph, target_set);

		string adderGraph = output_adder_graph(pipelined_adder_graph,true);

		REPORT(LogLevel::DETAIL, "adderGraph=" << adderGraph);

		ProcessIntConstMultShiftAdd(target,adderGraph,"",epsilon);

    ostringstream name;
    name << "IntConstMultRPAG_";
    for(auto t : target_set)
    {
      name << t << "_";
    }
    name << wIn;
    setName(name.str());
  }

	TestList IntConstMultShiftAddRPAG::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;

    vector<string> constantList; // The list of constants we want to test
//			constantList.push_back("0"); //should work in future but exceptions have to be implemented!
    constantList.push_back("1");
    constantList.push_back("16");
    constantList.push_back("123");
    constantList.push_back("3"); //1 adder
    constantList.push_back("7"); //1 subtractor
    constantList.push_back("11"); //smallest coefficient requiring 2 adders
    constantList.push_back("43"); //smallest coefficient requiring 3 adders
    constantList.push_back("683"); //smallest coefficient requiring 4 adders
    constantList.push_back("14709"); //smallest coefficient requiring 5 adders
    constantList.push_back("123456789"); //test a relatively large constant

    if(testLevel == TestLevel::QUICK)
    { // The quick unit tests
      int wIn=10;
      for(auto c:constantList) // test various constants
      {
        paramList.push_back(make_pair("wIn",  to_string(wIn)));
        paramList.push_back(make_pair("constant", c));
        testStateList.push_back(paramList);
        paramList.clear();
      }
    }
    else if(testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests

      for(int wIn=3; wIn<16; wIn+=4) // test various input widths
			{
				for(auto c:constantList) // test various constants
				{
					paramList.push_back(make_pair("wIn",  to_string(wIn)));
					paramList.push_back(make_pair("constant", c));
					testStateList.push_back(paramList);
					paramList.clear();
				}
			}
		}
		else
		{
			// finite number of random test computed out of testLevel
		}

		return testStateList;
	}

    OperatorPtr flopoco::IntConstMultShiftAddRPAG::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
    {
      int wIn, epsilon;

      ui.parseStrictlyPositiveInt( args, "wIn", &wIn );
      ui.parsePositiveInt( args, "errorBudget", &epsilon );

      string constListStr;
      ui.parseString(args, "constants", &constListStr);
      string constStr="";

      bool isSigned;
      ui.parseBoolean(args, "signed", &isSigned);

      try {
        list<mpz_class> constList; // the list of constants

        //parse list of constants:
        int pos=0;
        int nextPos;
        do{
          nextPos = constListStr.find(',', pos);
          if(nextPos == string::npos) nextPos = constListStr.length();

          constStr = constListStr.substr(pos, nextPos - pos);

          mpz_class constant(constStr);
          constList.push_back(constant);
          pos = nextPos+1;
        } while (nextPos != constListStr.length());

        if(constList.size() > 0)
        {
          return new IntConstMultShiftAddRPAG(parentOp, target, wIn, constList, isSigned, epsilon);
        }
        else
        {
          cerr << "Error: Empty constant list" << endl; //this should never happen
          exit(EXIT_FAILURE);
        }
      }
      catch (const std::invalid_argument &e) {
        cerr << "Error: Invalid constant " << constStr << endl;
        exit(EXIT_FAILURE);
      }

    }

   template<>
   const OperatorDescription<IntConstMultShiftAddRPAG> op_descriptor<IntConstMultShiftAddRPAG> {"IntConstMultShiftAddRPAG", // name
                            "Integer constant multiplication using shift and add using the RPAG algorithm", // description, string
                            "ConstMultDiv", // category, from the list defined in UserInterface.cpp
                            "", //seeAlso
                            "wIn(int): Input word size; \
                            constants(string): list of constants; \
                            signed(bool)=true: signedness of input and output; \
                            errorBudget(int)=0: Allowable error for truncated constant multipliers;",
	"Nope."};
}
#endif //defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)

