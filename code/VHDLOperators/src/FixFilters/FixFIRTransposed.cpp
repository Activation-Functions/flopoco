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

  FixFIRTransposed::FixFIRTransposed(OperatorPtr parentOp, Target* target, int wIn, int wOut, vector<int64_t> coeffs, string adder_graph, const string& graph_truncations, const string& sa_truncations, const int epsilon): Operator(parentOp, target), wIn(wIn), wOut(wOut), coeffs(coeffs), epsilon(epsilon)
	{
    useNumericStd();
    srcFileName="FixFIRTransposed";
    setName("FixFIRTransposed");

    noOfTaps = coeffs.size();
    assert(noOfTaps > 0);

    set<int64_t> coeffAbsSet;
    for(int i=0; i < noOfTaps; i++)
    {
      if(coeffs[i] != 0)
        coeffAbsSet.insert(abs(coeffs[i]));
    }

    if(adder_graph.compare("\"\"") == 0)
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

    vector<vector<int>> sa_truncations_vec(coeffs.size());
    if(!sa_truncations.empty())
    {
      parseSAtruncations(sa_truncations, sa_truncations_vec);

      //debug output:
      if (is_log_lvl_enabled(LogLevel::MESSAGE))
      {
        for(int i=0; i < sa_truncations_vec.size(); i++)
        {
          std::stringstream ss;
          ss << "Structural adder corresponding to coeff " << coeffs[i] << " is truncated by: ";
          for(auto truncation : sa_truncations_vec[i])
          {
            ss << truncation << " ";
          }
          ss << " bits";
          REPORT(LogLevel::MESSAGE,ss.str());
        }
      }
    }
    else
    {
      //fill with zeros
      for(int i=0; i < sa_truncations_vec.size(); i++)
      {
        sa_truncations_vec[i] = {0,0};
      }
    }

    addInput("X",wIn);

    stringstream parameters;
    parameters << "wIn=" << wIn << " graph=" << adder_graph;
    parameters << " truncations=" << graph_truncations;
    string inPortMaps = "X0=>X";
    stringstream outPortMaps;
    int i=0;
    map<int64_t,int> wC;
    for(auto c : coeffAbsSet)
    {
      wC[c] = wIn + ceil(log2(c ));
      stringstream sigName;
      sigName << "X_mult_" << c;
      outPortMaps << "R_c" << c << "=>" << declare(sigName.str(),wC[c]);
      getSignalByName(sigName.str())->setIsSigned(); //set output to signed
      if(i++ < coeffAbsSet.size() - 1)
        outPortMaps << ",";
    }

    //create the multiplier block:
    newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponent", parameters.str(), inPortMaps, outPortMaps.str());

    //compute the minimum word size of structural adders (without sa_truncations_vec)
    vector<int> wS(noOfTaps); // !!!
    for(int i=0; i < noOfTaps; i++)
    {
      int64_t sum=0;
      for(int j=0; j <= i; j++)
      {
        sum += abs(coeffs[j]);
      }
      wS[i] = wIn + ceil(log2(sum +1)); // the +1 is conservative making sure power-of-two coefficients work (with a strictly positive power-of-two coefficient, we could save one bit here)
      REPORT(DETAIL, "Word size of strucural adder " << i << " is " << wS[i] << " bits (range is " << -sum << " x 2^(wIn-1) to " << sum << " x (2^(wIn-1)-1)");
    }


    wOutFull=wS[noOfTaps-1]; //full precision output word size is identical to the one of the last structural adder:
    if(wOut == -1) //set output word size if not set by the user (default is -1)
    {
      wOut=wOutFull;
      this->wOut=wOutFull;
    }
    REPORT(DETAIL, "Setting output word size to " << wOut);
    addOutput("Y",wOut);

    //create the structural adders:
    if(noOfTaps == 1)
    {
      if(coeffs[0] == 0)
      {
        vhdl << tab << "Y <= (others => '0');" << endl; //this is the very trivial case of a 1-tap filter with coefficient 0
      }
      else
      {
        vhdl << tab << "Y <= X_mult_" << abs(coeffs[0]) << ";" << endl; //this is the very trivial case of a 1-tap filter
      }
    }
    else
    {
      //the first structural adder gets a special treatment:
      //Here, an adder could potentially saved when considering negative coefficients in the adder graph
      vhdl << endl << tab << declare("s0",wS[0]) << " <= ";
      if(coeffs[0] == 0)
      {
        vhdl << "(others => '0');" << endl;
      }
      else
      {
        vhdl << "std_logic_vector(" << ((coeffs[0] < 0) ? "-" : "") << "resize(signed(X_mult_" << abs(coeffs[0]) << ")," << wS[0] << "));" << endl;
      }


      for(int i=1; i < noOfTaps; i++)
      {
        addRegisteredSignalCopy(join("s", i-1) + "_delayed", join("s", i-1),Signal::asyncReset);

        vhdl << endl;

        //pass LSB bits of one of the inputs in case of truncations
        int wSATruncatedfromSA=sa_truncations_vec[i][0]; //input from previous adder
        int wSATruncatedfromMCM=sa_truncations_vec[i][1]; //input from multiplier block
        int wSALSBsPassed; //no of bits that are passed to the LSBs


        wSALSBsPassed=abs(sa_truncations_vec[i][1] - sa_truncations_vec[i][0]);
        int bitsEliminated = min(wSATruncatedfromSA, wSATruncatedfromMCM);
        int savedFAs = bitsEliminated + wSALSBsPassed;
        addComment("structural adder " + to_string(i) + " (input from previous adder truncated by " + to_string(wSATruncatedfromSA) + " bits, input from multiplier block truncated by " + to_string(wSATruncatedfromMCM) + " bits, saving " + to_string(savedFAs) + " full adders):");
        if((wSATruncatedfromSA > 0) || (wSATruncatedfromMCM > 0))
        {
          if(wSATruncatedfromSA < wSATruncatedfromMCM)
          {
            //the previous structural adder is passed to the LSBs
            vhdl << tab << declare("s" + to_string(i) + "_LSBs", wSALSBsPassed) << " <= " << "s" + to_string(i - 1) << "_delayed(" << wSATruncatedfromSA + wSALSBsPassed - 1 << " downto " << wSATruncatedfromSA << ")" << ";" << endl;
            wSATruncatedfromSA += wSALSBsPassed;
          }
          else if(wSATruncatedfromSA > wSATruncatedfromMCM)
          {
            //the multiplier block output is passed to the LSBs
            vhdl << tab << declare("s" + to_string(i) + "_LSBs", wSALSBsPassed) << " <= ";
            if(coeffs[i] == 0)
            {
              vhdl << "(others => '0');" << endl;
            }
            else
            {
              vhdl << "X_mult_" << abs(coeffs[i]) << "(" << wSATruncatedfromMCM + wSALSBsPassed - 1 << " downto " << wSATruncatedfromMCM << ")" << ";" << endl;
            }

            wSATruncatedfromMCM += wSALSBsPassed;
          }
          else
          {
            //nothing is passed, nothing to do
          }
        }

        bool msbLsbSplitNecessary = (wSALSBsPassed > 0) || (bitsEliminated > 0);
        vhdl << tab << declare("s" + to_string(i) + (msbLsbSplitNecessary ? "_MSBs" : ""), wS[i] - savedFAs)
             << " <= std_logic_vector("
             << "resize(signed("
             << "s" + to_string(i-1) << "_delayed"
             << (wSATruncatedfromSA > 0 ? "(" + to_string(wS[i - 1] - 1) + " downto " + to_string(wSATruncatedfromSA) + ")" : "")
             << ")," << wS[i] - savedFAs
             << ")";
        if(coeffs[i] == 0)
        {
          vhdl << "); -- coefficient is zero, nothing to add" << endl;
        }
        else
        {
          vhdl << " " << (coeffs[i] < 0 ? "-" : "+") << " "
               << "resize(signed("
               << "X_mult_" << abs(coeffs[i])
               << (wSATruncatedfromMCM > 0 ? "(" + to_string(wC[abs(coeffs[i])] - 1) + " downto " + to_string(wSATruncatedfromMCM) + ")" : "")
               << ")," << wS[i] - savedFAs << "));" << endl;
        }

        if(msbLsbSplitNecessary)
        {
          vhdl << tab << declare("s" + to_string(i),wS[i]) << " <= "
               << "s" << i << "_MSBs"
               << (wSALSBsPassed > 0 ? " & s" + to_string(i) + "_LSBs" : "")
               << (bitsEliminated > 0 ? " & " + zg(savedFAs-wSALSBsPassed) : "")
               << ";" << endl;
        }
      }
      if(wOut == wOutFull)
      {
        vhdl << tab << "Y <= s" << noOfTaps-1 << ";" << endl; //the output of the filter
      }
      else
      {
        vhdl << tab << "Y <= std_logic_vector(signed(s" << noOfTaps-1 << "(" << wS[noOfTaps-1]-1 << " downto " << wS[noOfTaps-1]-wOut << ")) + signed(\"0\" & s" << noOfTaps-1 << "(" << wS[noOfTaps-1]-wOut-1 << " downto " << wS[noOfTaps-1]-wOut-1 << ")))" << ";" << endl; //the output of the filter
      }
    }

    // initialize stuff for emulate
    xHistory.resize(noOfTaps, 0); //resize history
    currentIndex=0;

  }

  void FixFIRTransposed::parseSAtruncations(const string& sa_truncations, vector<vector<int>>& truncations)
  {
    list<string> itemList;
    {
      std::stringstream ss;
      ss.str(sa_truncations);
      std::string item;
      try {
        while (std::getline(ss, item, '|')) {
          itemList.push_back(item);
        }
      }
      catch(std::exception &s){
        THROWERROR("Problem in parseSAtruncations for "<< sa_truncations << endl << "got exception : " << s.what());
      }
    }

    int i=0;
    for(auto item : itemList)
    {
      std::stringstream ss;
      ss.str(item);
      std::string subitem;
      vector<int> inputTruncations;
      try {
        while (std::getline(ss, subitem, ',')) {
          int truncation = stoi(subitem);
          inputTruncations.push_back(truncation);
        }
        if(inputTruncations.size() != 2)
          THROWERROR("Currently, only 2-input adders are supported, got truncations for " << inputTruncations.size() << " input(s)!");

        if(i >= truncations.size())
          THROWERROR("Got too many structural adder truncations, should get " << truncations.size());

        truncations[i++]=inputTruncations;
      }
      catch(std::exception &s){
        THROWERROR("Problem in parseSAtruncations for "<< item << endl << "got exception : " << s.what());
      }
    }

    if(i < truncations.size())
      THROWERROR("Got too few structural adder truncations, got " << i << ", should get " << truncations.size());

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

//#define DEBUG
#ifdef DEBUG
    cerr << "emulate inputs (currentIndexMod=" << currentIndexMod << ")=";
#endif
    // Not completely optimal in terms of object copies...
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
    cerr << ": y = " << y;
#endif


//    if(currentIndex > noOfTaps) //ignore the first noOfTaps cycles as registers has to be filled with something useful

//#define ENUMERATE_ALL_VALUES
#ifdef ENUMERATE_ALL_VALUES //the old interface
#ifdef DEBUG
//    cout << "!!! the old interface !!!" << endl;
#endif
    tc->addExpectedOutput ("Y", signedToBitVector(y, wOut));

    if(epsilon > 0)
    {
      //Probably not the most efficient way for large epsilons...
      for(int e=1; e <= epsilon; e++)
      {
        tc->addExpectedOutput("Y", signedToBitVector(y+e, wOut));
        tc->addExpectedOutput("Y", signedToBitVector(y-e, wOut));
      }
    }
#else
#ifdef DEBUG
//    cout << "!!! the new interface !!! [" << y-epsilon << " " << y+epsilon<< "]" <<endl;
#endif
    if(wOut != wOutFull)
    {
      y = y >> (wOutFull-wOut);
#ifdef DEBUG
      cerr << ", y (truncated) = " << y;
#endif

    }
    if(epsilon > 0)
    {
      tc->addExpectedOutputInterval("Y",  y - epsilon, y + epsilon, TestCase::signed_interval); // <- this is one that should work
//      tc->addExpectedOutputInterval("Y", signedToBitVector(y - epsilon, wOut), signedToBitVector(y + epsilon, wOut), TestCase::signed_interval);
//      tc->addExpectedOutputInterval("Y", signedToBitVector(y - epsilon, wOut), signedToBitVector(y + epsilon, wOut), TestCase::unsigned_interval);
    }
    else
    {
      tc->addExpectedOutput ("Y", signedToBitVector(y, wOut));
      if(wOut != wOutFull)
      {
        //this is the case of faithful rounding, add y+1 as well
        tc->addExpectedOutput ("Y", signedToBitVector(y+1, wOut));
      }
    }
#endif

#ifdef DEBUG
    cerr << endl;
#endif

    currentIndex++;

	};

	OperatorPtr FixFIRTransposed::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
    int wIn;
    ui.parseInt(args, "wIn", &wIn);

    int wOut;
    ui.parseInt(args, "wOut", &wOut);

		vector<string> coeffs;
		ui.parseColonSeparatedStringList(args, "coeff", &coeffs);

    vector<int64_t> coeffsInt(coeffs.size());
    for(int i=0; i < coeffs.size(); i++)
    {
      coeffsInt[i] = stoll(coeffs[i]);
    }

    string adder_graph;
    ui.parseString(args, "graph", &adder_graph);

    string graph_truncations;
    ui.parseString(args, "graph_truncations", &graph_truncations);

    if (graph_truncations == "\"\"") {
      graph_truncations = "";
    }

    string sa_truncations;
    ui.parseString(args, "sa_truncations", &sa_truncations);

    if (sa_truncations == "\"\"") {
      sa_truncations = "";
    }

    int epsilon;
    ui.parseInt(args, "epsilon", &epsilon);

    OperatorPtr tmpOp = new FixFIRTransposed(parentOp, target, wIn, wOut, coeffsInt, adder_graph, graph_truncations, sa_truncations, epsilon);

		return tmpOp;
	}

	template <>
	const OperatorDescription<FixFIRTransposed> op_descriptor<FixFIRTransposed> {
	    "FixFIRTransposed", // name
	    "A fix-point Finite Impulse Filter generator in transposed form using shif-and-add.",
	    "FiltersEtc", // categories
	    "",
	    "wIn(int): input word size in bits;\
                  wOut(int)=-1: output word size in bits, -1 means automatic determination without truncation, if wOut is less then necessary it will be rounded;\
                 coeff(string): colon-separated list of integer coefficients. Example: coeff=\"123:321:123\";\
                 graph(string)=\"\": Realization string of the adder graph;\
                 epsilon(int)=0: Allowable error for truncated constant multipliers (currently only used to check error for given truncations in testbench); \
                 graph_truncations(string)=\"\": provides the truncations for intermediate values of the adder graph (format: const1,stage:trunc_input_0,trunc_input_1,...|const2,stage:trunc_input_0,trunc_input_1,...)\";\
                 sa_truncations(string)=\"\": provides the truncations for the strucutal adder (format: sa_1_trunc_input_0,sa_1_trunc_input_1|...|sa_2_trunc_input_0,sa_2_trunc_input_1,...|...)",
	    ""
  };
}

