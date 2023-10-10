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

  FixFIRTransposed::FixFIRTransposed(OperatorPtr parentOp, Target* target, int wIn, vector<int64_t> coeffs, string adder_graph): Operator(parentOp, target), wIn(wIn), coeffs(coeffs)
	{
    useNumericStd();
    srcFileName="FixFIRTransposed";
    setName("FixFIRTransposed");

    noOfTaps = coeffs.size();
    assert(noOfTaps > 0);

    set<int64_t> coeffAbsSet;
    for(int i=0; i < noOfTaps; i++)
    {
      coeffAbsSet.insert(abs(coeffs[i]));
    }

    if(adder_graph.compare("false") == 0)
    {
      #if defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB)
        REPORT(LogLevel::MESSAGE,"No adder graph was given, computing the adder graph by RPAG");

        PAGSuite::rpag *rpag = new PAGSuite::rpag(); //default is RPAG with 2 input adders

        for(int64_t c : coeffAbsSet)
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
    int i=0;
    map<int64_t,int> wC;
    for(auto c : coeffAbsSet)
    {
      wC[c] = wIn + ceil(log2(c+1));
      stringstream sigName;
      sigName << "X_mult_" << c;
      outPortMaps << "R_c" << c << "=>" << declare(sigName.str(),wC[c]);
      if(i++ < coeffAbsSet.size() - 1)
        outPortMaps << ",";
    }

    //create the multiplier block:
    newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponent", parameters.str(), inPortMaps, outPortMaps.str());

    //compute the minimum word size of structural adders (without truncations)
    vector<int> wS(noOfTaps); // !!!
    for(int i=0; i < noOfTaps; i++)
    {
      int64_t sum=0;
      for(int j=0; j <= i; j++)
      {
        sum += abs(coeffs[j]);
      }
      wS[i] = wIn + ceil(log2(sum));
      REPORT(DETAIL, "Word size of strucural adder " << i << " is " << wS[i] << " bits (range is " << -sum << " x 2^(wIn-1) to " << sum << " x (2^(wIn-1)-1)");
    }

    //output word size is identical to the one of the last structural adder:
    wOut=wS[noOfTaps-1];
    addOutput("Y",wOut);

    //create the structural adders:
    if(noOfTaps == 1)
    {
      vhdl << tab << "Y <= X_mult_" << abs(coeffs[0]) << ";" << endl; //this is the very trivial case of a 1-tap filter
    }
    else
    {
      //the first structural adder gets a special treatment:
      vhdl << tab << declare("s0",wS[0]) << " <= X_mult_" << abs(coeffs[0]) << ";" << endl;
      for(int i=1; i < noOfTaps; i++)
      {
        addRegisteredSignalCopy(join("s", i-1) + "_delayed", join("s", i-1),Signal::asyncReset);
        vhdl << tab << declare("s" + to_string(i),wS[i]) << " <= std_logic_vector(resize(signed(" << "s" + to_string(i-1) << "_delayed)," << wS[i] << ") "  << (coeffs[i] < 0 ? "-" : "+") << " resize(signed(X_mult_" << abs(coeffs[i]) << ")," << wS[i] << "));" << endl;
      }
      vhdl << tab << "Y <= s" << noOfTaps-1 << ";" << endl; //this is the very trivial case of a 1-tap filter
    }

    // initialize stuff for emulate
    xHistory.resize(noOfTaps, 0); //resize history
    currentIndex=0;

  }

  void FixFIRTransposed::buildStandardTestCases(TestCaseList * tcl)
  {
    TestCase *tc;
    tc = new TestCase(this);
    tc->addComment("Test Impulse response with 1 as input");
    tc->addInput("X", 1);
    emulate(tc);
    tcl->add(tc);
    for(int i = 0; i < noOfTaps; i++)
    {
      tc = new TestCase(this);
      tc->addComment("Test Impulse response with 1 as input");
      tc->addInput("X", 0);
      emulate(tc);
      tcl->add(tc);
    }

    tc = new TestCase(this);
    tc->addComment("Test Impulse response with 1 as input");
    tc->addInput("X", (1<<(wIn-1))-1);
    emulate(tc);
    tcl->add(tc);

    for(int i = 0; i < noOfTaps; i++)
    {
      tc = new TestCase(this);
      tc->addComment("Test Impulse response with max input");
      tc->addInput("X", 0);
      emulate(tc);
      tcl->add(tc);
    }

  }

	void FixFIRTransposed::emulate(TestCase * tc){
		mpz_class x = tc->getInputValue("X"); 		// get the input bit vector as an integer

    int currentIndexMod=currentIndex%noOfTaps; //  circular buffer to store the inputs
    xHistory[currentIndexMod] = x;

		// Not completely optimal in terms of object copies...
#ifdef DEBUG
    cerr << "emulate inputs (currentIndexMod=" << currentIndexMod << ")=";
#endif
		vector<mpz_class> inputs;
		for (int i=0; i< noOfTaps; i++)
    {
			x = xHistory[(currentIndexMod+noOfTaps-i)%noOfTaps];
      mpz_class xSigned = bitVectorToSigned(x, wIn); 						// convert it to a signed mpz_class
			inputs.push_back(xSigned);
#ifdef DEBUG
      cerr << xSigned << " ";
#endif
		}

    mpz_class y=0;
    for (int i=0; i< noOfTaps; i++)
    {
      mpz_class c(((signed long int) coeffs[noOfTaps-i-1]));
      y = y + inputs[i] * c;
    }
#ifdef DEBUG
    cerr << ": y = " << y << endl;
#endif


//    if(currentIndex > noOfTaps) //ignore the first noOfTaps cycles as registers has to be filled with something useful
		  tc->addExpectedOutput ("Y", signedToBitVector(y, wOut));

    currentIndex++;

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
	    ""
  };
}

