#include <iostream>
#include <iomanip>
#include <sstream>
#include <cstdlib>

#include "flopoco/UserInterface.hpp"
#include "flopoco/FixFilters/FixFIRTransposed.hpp"

#if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)
#include "pagsuite/log2_64.h"
#include "pagsuite/fundamental.h"
#include "pagsuite/rpag.h"
#include "pagsuite/rpag_functions.h"
#endif

using namespace std;

namespace flopoco {

  FixFIRTransposed::FixFIRTransposed(OperatorPtr parentOp, Target* target, int wIn, vector<int64_t> coeffs, string adder_graph): Operator(parentOp, target), wIn(wIn)
	{
    srcFileName="FixFIRTransposed";
    setName("FixFIRTransposed");

    if(adder_graph.compare("false") == 0)
    {
      #if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)
        REPORT(LogLevel::MESSAGE,"No adder graph was given, computing the adder graph by RPAG");

        PAGSuite::rpag *rpag = new PAGSuite::rpag(); //default is RPAG with 2 input adders

        for(int64_t c : coeffs)
          rpag->target_set->insert(c);

        PAGSuite::global_verbose = static_cast<int>(get_log_lvl())-1; //set rpag to one less than verbose of FloPoCo

        PAGSuite::cost_model_t cost_model = PAGSuite::LL_FPGA;// with default value
        rpag->input_wordsize = wIn;
        rpag->set_cost_model(cost_model);
        rpag->optimize();

        vector<set<int64_t>> pipeline_set = rpag->get_best_pipeline_set();

        list<PAGSuite::realization_row<int64_t> > pipelined_adder_graph;
        PAGSuite::pipeline_set_to_adder_graph(pipeline_set, pipelined_adder_graph, true, rpag->get_c_max());
        PAGSuite::append_targets_to_adder_graph(pipeline_set, pipelined_adder_graph, *(rpag->target_set));
        adder_graph = PAGSuite::output_adder_graph(pipelined_adder_graph,true);

        REPORT(LogLevel::MESSAGE,"Got adder graph " << adder_graph);

      #else
        REPORT(LogLevel::ERROR,"No adder graph was given but PAGlib is missing to use RPAG, please build FloPoCo with PAGlib");
      #endif

    }

    string trunactionStr=""; //TODO: fill

    addInput("X",wIn);

    stringstream parameters;
    parameters << "wIn=" << wIn << " graph=" << adder_graph;
    parameters << " truncations=" << trunactionStr;
    string inPortMaps = "X0=>X";
    stringstream outPortMaps;
    for(auto c : coeffs)
    {
      int wC = wIn;
      stringstream sigName;
      sigName << "X_mult_" << (c < 0 ? "m" : "") << abs(c);
      outPortMaps << "R_c" << (c < 0 ? "m" : "") << abs(c) << "=>" << declare(sigName.str(),wC) << " ";
    }
    cerr << "outPortMaps=" << outPortMaps.str() << endl;

    newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponent", parameters.str(), inPortMaps, outPortMaps.str());


  }


	void FixFIRTransposed::emulate(TestCase * tc){
		mpz_class sx = tc->getInputValue("X"); 		// get the input bit vector as an integer
		xHistory[currentIndex] = sx;

		// Not completely optimal in terms of object copies...
		vector<mpz_class> inputs;
		for (int i=0; i< n; i++)	{
			sx = xHistory[(currentIndex+n-i)%n];
			inputs.push_back(sx);
		}
//		pair<mpz_class,mpz_class> results = refFixSOPC-> computeSOPCForEmulate(inputs);

//		tc->addExpectedOutput ("R", results.first);
//		tc->addExpectedOutput ("R", results.second);
		currentIndex=(currentIndex+1)%n; //  circular buffer to store the inputs

	};

	OperatorPtr FixFIRTransposed::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wIn;
		ui.parseInt(args, "wIn", &wIn);
		vector<string> coeffs;
		ui.parseColonSeparatedStringList(args, "coeff", &coeffs);

    vector<int64_t> coeffsInt(coeffs.size());
    for(int i=0; i < coeffs.size(); i++)
    {
      coeffsInt[i] = stoll(coeffs[i]);
    }

    string adder_graph;
    ui.parseString(args, "graph", &adder_graph);

		OperatorPtr tmpOp = new FixFIRTransposed(parentOp, target, wIn, coeffsInt, adder_graph);

		return tmpOp;
	}

	template <>
	const OperatorDescription<FixFIRTransposed> op_descriptor<FixFIRTransposed> {
	    "FixFIRTransposed", // name
	    "A fix-point Finite Impulse Filter generator in transposed form using shif-and-add.",
	    "FiltersEtc", // categories
	    "",
	    "wIn(int): input word size in bits;\
                 coeff(string): colon-separated list of integer coefficients. Example: coeff=\"123:321:123\";\
                 graph(string)=false: Realization string of the adder graph",
	    ""};
}

