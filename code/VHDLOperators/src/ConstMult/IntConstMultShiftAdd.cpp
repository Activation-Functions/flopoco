#include "flopoco/ConstMult/IntConstMultShiftAdd.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"

#if defined(HAVE_PAGLIB)

#include <iostream>
#include <sstream>
#include <string>

#include <cstdlib>
#include <cassert>
#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <sys/types.h>
#include <fcntl.h>
#include <filesystem>
/*
#include <map>
#include <vector>
#include <set>

#include "flopoco/ConstMult/adder_cost.hpp"
*/

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitraly large
   entries */
#include "gmp.h"
#include "mpfr.h"
#include "pagsuite/log2_64.h"
// include the header of the Operator
#include "flopoco/PrimitiveComponents/Primitive.hpp"
//#include "rpag/rpag.h"
#include "flopoco/IntAddSubCmp/IntAddSub.hpp"

#include "flopoco/ConstMult/WordLengthCalculator.hpp"

using namespace std;
using namespace PAGSuite;


namespace flopoco {


	IntConstMultShiftAdd::IntConstMultShiftAdd(
	    Operator* parentOp,
	    Target* target,
	    int wIn_,
	    string adder_graph_str,
      bool isSigned,
	    int epsilon_,
	    string truncations
	)
	    : Operator(parentOp, target),
	      wIn(wIn_),
        isSigned(isSigned),
	      epsilon(epsilon_)
	{

		ostringstream name;
		name << "IntConstMultShiftAdd_" << wIn;
		setNameWithFreqAndUID(name.str());

    if(adder_graph_str.empty()) return; //in case the realization string is not defined, don't further process it.

    ProcessIntConstMultShiftAdd(target,adder_graph_str,truncations,epsilon_);
  };

  void IntConstMultShiftAdd::ProcessIntConstMultShiftAdd(Target* target, string adder_graph_str, string truncations, int epsilon)
  {
    if(adder_graph_str.empty()) return; //in case the realization string is not defined, don't further process it.

    useNumericStd();

    REPORT(DEBUG,"reading adder_graph_str=" << adder_graph_str);

    if(is_log_lvl_enabled(LogLevel::DEBUG))
      adder_graph.quiet = false; //enable debug output
    else
      adder_graph.quiet = true; //disable debug output, except errors

    REPORT(LogLevel::VERBOSE, "parse graph...")
    bool validParse = adder_graph.parse_to_graph(adder_graph_str);

    adder_graph.check_and_correct();
//    adder_graph.normalize_graph(); //normalize corrects order of signs such that an adder's first input is always positive (if possible)
//    adder_graph.check_and_correct();

    if(validParse) {
      adder_graph.drawdot("pag_input_graph.dot");

      stringstream outstream;
      adder_graph.writesyn(outstream);
      REPORT(DEBUG,"Parsed graph is " << outstream.str());
//      REPORT(DEBUG,"Parsed graph is " << adder_graph.get_adder_graph_as_string());

      if(is_log_lvl_enabled(LogLevel::DEBUG))
        adder_graph.print_graph();

      isTruncated=false;
      if(!truncations.empty() || (epsilon > 0))
      {
        isTruncated=true;
        if(!truncations.empty())
        {
          REPORT(LogLevel::DETAIL,  "Parsing truncations...");

          parseTruncation(truncations);
        }
        if(epsilon > 0)
        {
          REPORT(LogLevel::DETAIL,  "Found non-zero epsilon=" << epsilon << ", computing word sizes of truncated MCM");

#if defined(HAVE_PAGLIB) && defined(HAVE_SCALP)
          WordLengthCalculator wlc = WordLengthCalculator(adder_graph, wIn, epsilon, target);
          wordSizeMap = wlc.optimizeTruncation();

          REPORT(LogLevel::DETAIL, "Finished computing word sizes of truncated MCM");

#else
          THROWERROR("The word length's in IntConstMultShiftAdd can not be obtained without ScaLP library, please build with ScaLP library.");
#endif
        }

        REPORT(LogLevel::DETAIL, "Using the following truncations (format: \"const, stage: trunc_input_0, trunc_input_1, ...)\":");
        if(is_log_lvl_enabled(LogLevel::DETAIL))
        {
          for(auto & it : wordSizeMap) {
            std::cout << it.first.first << ", " << it.first.second << ": ";
            for(auto & itV : it.second)
              std::cout << itV << " ";
            std::cout << std::endl;
          }
        }

      }

      generateAdderGraph(&adder_graph);
    }
  }

  void IntConstMultShiftAdd::parseTruncation(string truncationList)
  {
    static const string fieldDelimiter{"|"};
    auto getNextField = [](string& val)->string{
      long unsigned int offset = val.find(fieldDelimiter);
      string ret = val.substr(0, offset);
      if (offset != string::npos) {
        offset += 1;
      }
      val.erase(0, offset);
      return ret;
    };
    while(truncationList.length() > 0)
      parseTruncationRecord(getNextField(truncationList));
  }

  void IntConstMultShiftAdd::parseTruncationRecord(string record)
  {
    static const string identDelimiter{':'};
    static const string fieldDelimiter{','};

    auto getNextField = [](string& val)->mpz_class{
      long unsigned int offset = val.find(fieldDelimiter);
      string ret = val.substr(0, offset);
      if (offset != string::npos) {
        offset += 1;
      }
      val.erase(0, offset);
      return mpz_class(ret);
    };

    long unsigned int  offset = record.find(identDelimiter);
    if (offset == string::npos) {
      throw string{"IntConstMultShiftAdd::TruncationRegister::parseRecord : "
      "wrong format "} + record;
    }
    string recordIdStr = record.substr(0, offset);
    record.erase(0, offset + 1);
    string& valuesStr = record;

    mpz_class factor = getNextField(recordIdStr);
    int stage = getNextField(recordIdStr).get_si();

    vector<int> truncats;

    while(valuesStr.length() > 0) {
      truncats.push_back(getNextField(valuesStr).get_si());
    }

    wordSizeMap.insert({make_pair(factor, stage), truncats});

    if(is_log_lvl_enabled(DEBUG))
    {
      cerr << "(" << factor << "," << stage << "):";
      for(int t : truncats)
        cerr << t << " ";
      cerr << endl;
    }
  }

  void IntConstMultShiftAdd::generateAdderGraph(PAGSuite::adder_graph_t* adder_graph)
  {
    REPORT(DETAIL,"Generating adder graph using " << (getTarget()->plainVHDL() ? "plain" : "structured") << "VHDL");
    noOfInputs=0;
    noOfOutputs=0;
    for(adder_graph_base_node_t* node : adder_graph->nodes_list)
    {
      if(!isSigned) //sanity check: in unsigned case, check all factors to be positive
      {
        for(int c=0; c < node->output_factor.size(); c++)
        {
          for(int i=0; i < node->output_factor[c].size(); i++)
          {
            if(node->output_factor[c][i] < 0)
            {
              THROWERROR("Found negative factor " << node->output_factor << " in an unsigned operator, select isSigned=true to build this operator");
            }

/*
            if(is_a<conf_adder_subtractor_node_t>(*node))
            {
              conf_adder_subtractor_node_t cnode = ((conf_adder_subtractor_node_t*) node);
              for(int c=0; c < cnode->input_is_negative.size(); c++)
              {
                for(int i = 0; i < cnode->inputs.size(); i++)
                {
                  if(cnode->input_is_negative[c][i]) {
                    THROWERROR("Found negative factor " << node->output_factor << " in an unsigned operator, select isSigned=true to build this operator");
                  }
                }
              }
            }
*/
          }
        }
      }

      //check no of configurations:
      if(noOfConfigurations == -1)
      {
        noOfConfigurations = node->output_factor.size();
        REPORT(DETAIL,"Found " << noOfConfigurations << " different configuration(s).");

        if(noOfConfigurations > 1)
        {
          //more than one configuration found, declare select input:
          wSel = ceil(log2(noOfConfigurations));
          addInput(generateSelectName(),wSel);
        }
        else
        {
          wSel = 0;
        }
      }
      else
      {
        //check for consistency
        if(noOfConfigurations != node->output_factor.size())
        {
          THROWERROR("Missmatch of no of configurations in node computing " << node->output_factor << "(" << node->output_factor.size() << ") and other nodes (" << noOfConfigurations << ")");
        }
      }


      if(is_a<input_node_t>(*node))
      {
        generateInputNode((input_node_t*) node);
  //      input_signals.push_back(generateSignalName(node->output_factor, node->stage));
        noOfInputs++;
      }
      else if(is_a<output_node_t>(*node))
      {
        generateOutputNode((output_node_t*) node);

        noOfOutputs++;
      }
      else if(is_a<adder_subtractor_node_t>(*node))
      {
        generateAdderSubtractorNode((adder_subtractor_node_t*) node);
      }
      else if(is_a<register_node_t>(*node))
      {
        generateRegisterNode((register_node_t*) node);
      }
      else if(is_a<mux_node_t>(*node))
      {
        generateMuxNode((mux_node_t*) node);
      }
      else if(is_a<conf_adder_subtractor_node_t>(*node))
      {
        generateConfAdderSubtractorNode((conf_adder_subtractor_node_t*) node);
      }
      else
      {
        cerr << "Error: Unknown node " << node << endl;
      }

    }

    if(noOfOutputs==0)
    {
      THROWERROR("Adder graph does not contain any output node");
    }
  }

  void IntConstMultShiftAdd::generateInputNode(PAGSuite::input_node_t* node)
  {
    assert(node->output_factor.size() > 0);
    int inputNo=0;
    for(int i=0; i < node->output_factor[0].size(); i++)
    {
      if(node->output_factor[0][i] == 1)
        break;
      inputNo++;
    }

    string inputname = "X" + to_string(inputNo);
    REPORT(DEBUG,"processing input " << node->output_factor << ", adding input " << inputname);
    addInput(inputname, wIn);

    vhdl << tab << declare(generateSignalName(node->output_factor,node->stage),wIn) << " <= " << inputname << ";" << endl;

  }

  void IntConstMultShiftAdd::generateOutputNode(PAGSuite::output_node_t* node)
  {
    string name = "R_" + factorToString(node->output_factor);
//    output_factors.push_back(node->output_factor);

    int wOut = computeWordSize(node->output_factor, wIn);
    REPORT(DEBUG,"generating output " << name << " storing " << node->output_factor << " in stage " << node->stage << " using " << wOut << " bits");
    addOutput(name, wOut);

    string signed_str = isSigned ? "signed" : "unsigned";
    vhdl << tab << name << " <= std_logic_vector(" << signed_str << "(shift_left(resize(" << signed_str << "(" << generateSignalName(node->input->output_factor,node->input->stage) << ")," << wOut << ")," << node->input_shift << ")));" << endl;
  }

  void IntConstMultShiftAdd::generateRegisterNode(PAGSuite::register_node_t* node)
  {
    string name = generateSignalName(node->output_factor,node->stage);

    int result_word_size = computeWordSize(node->output_factor, wIn);
    REPORT(DEBUG,"generating register " << name << " storing " << node->output_factor << " in stage " << node->stage << " using " << result_word_size << " bits");

    vhdl << tab << declare(name, result_word_size) << " <= " << generateSignalName(node->input->output_factor, node->stage - 1) << ";" << endl;
  }

  void IntConstMultShiftAdd::generateConfAdderSubtractorNode(PAGSuite::conf_adder_subtractor_node_t* node)
  {
    generateAdderSubtractorNode(node);
  }

  void IntConstMultShiftAdd::generateAdderSubtractorNode(PAGSuite::adder_subtractor_node_t* node)
  {
    REPORT(DEBUG,"normalizing node " << node->output_factor << " in stage " << node->stage);
    bool nodeIsNormalized = adder_graph.normalize_node(node); //is true when normalization was successful, i.e., first input of adder is positive

    if(!nodeIsNormalized) cerr << endl << endl <<  "!!!!!!! node was not normalized !!!!!" << endl << endl;

    //The following steps are performed
    //0: Determine constants: configurability, input word sizes of arguments X, Y and Z, output word size
    bool isConfigurableAddSub=false;
    conf_adder_subtractor_node_t* cnode = nullptr; //just for easier access
    if(is_a<conf_adder_subtractor_node_t>(*node))
    {
      isConfigurableAddSub=true;
      cnode = ((conf_adder_subtractor_node_t*) node);
    }

    int wAddOut = computeWordSize(node->output_factor, wIn);
    vector<int> wAddIn(node->inputs.size());
    for(int i=0; i < node->inputs.size(); i++)
    {
      wAddIn[i] = computeWordSize(node->inputs[i]->output_factor, wIn);
    }

    string signalNameOut = generateSignalName(node->output_factor,node->stage);
    declare(signalNameOut,wAddOut); //output signal of the adder equals signalName
    string signalNameIn[node->inputs.size()];
    for(int i=0; i < node->inputs.size(); i++)
    {
      signalNameIn[i] = signalNameOut + "_in" + to_string(i);

      vhdl << tab << declare(signalNameIn[i], wAddIn[i]) << " <= ";
      vhdl << generateSignalName(node->inputs[i]->output_factor, node->inputs[i]->stage) << ";" << endl;
    }

    //some detailed output about what is computed:
    if(is_log_lvl_enabled(DETAIL))
    {
      cerr << std::filesystem::path{__FILE__}.filename() << ": ";

      if(!isConfigurableAddSub)
      {
        //the simple add/sub with static signs
        cerr << "processing adder/subtractor computing [" << node->output_factor << "] (stage " << node->stage << ") = ";
        for(int i=0; i < node->inputs.size(); i++)
        {
          if(node->input_is_negative[i])
          {
            cerr << " - ";
          }
          else
          {
            if(i > 0) cerr << " + ";
          }
          cerr << "[" << node->inputs[i]->output_factor << "] (stage " << node->inputs[i]->stage << ")";
          if(node->input_shifts[i] > 0)
          {
            cerr << " << " << node->input_shifts[i];
          }
        }
      }
      else
      {
        //for configurable add/sub, the sign depends on configuration
        cerr << "processing configurable adder/subtractor computing" << endl;
        for(int c=0; c < cnode->input_is_negative.size(); c++)
        {
          cerr << "[" << node->output_factor << "] (stage " << node->stage << ") = ";
          for (int i = 0; i < cnode->inputs.size(); i++) {
            if (cnode->input_is_negative[c][i])
            {
              cerr << " - ";
            }
            else
            {
              if (i > 0) cerr << " + ";
            }
            cerr << "[" << cnode->inputs[i]->output_factor << "] (stage " << cnode->inputs[i]->stage << ")";
            if (cnode->input_shifts[i] > 0)
            {
              cerr << " << " << cnode->input_shifts[i];
            }
          }
          cerr << " in configuration " << c << endl;
        }
      }

      cerr << " using " << wAddOut << " output bits and ";
      for(int i=0; i < node->inputs.size(); i++)
      {
        cerr << wAddIn[i];
        if(i < node->inputs.size()-1) cerr << ", ";
      }
      cerr << " input bits, respectively" <<  endl;
    }

    //Step 1: create truncated versions of the input signals (if any), adjust input word sizes
    vector<int> truncations(node->inputs.size(),0);
    if(isTruncated)
    {
      mpz_class of((signed long int) node->output_factor[0][0]);
      truncations = wordSizeMap.at(make_pair(of,node->stage));
      for(int i=0; i < node->inputs.size(); i++)
      {
        wAddIn[i] -= truncations[i];  //set input word size to the truncated size
        node->input_shifts[i] += truncations[i]; //adjust shifts with respect to the truncated signal
        vhdl << tab << declare(signalNameIn[i] + "_trunc", wAddIn[i]) << " <= " << signalNameIn[i] << range(wAddIn[i] + truncations[i] - 1, truncations[i]) << ";" << endl;
      }
    }

    //determine zero LSB positions (typically come from truncations)
    int zeroLSBs=INT_MAX;
    for(int i=0; i < node->input_shifts.size(); i++)
    {
      if(node->input_shifts[i] < zeroLSBs)
      {
        zeroLSBs = node->input_shifts[i];
      }
    }
    if(zeroLSBs > 0)
    {
      REPORT(DETAIL,"There are " << zeroLSBs << " LSB positions in the result that are zero");
      for(int i=0; i < node->input_shifts.size(); i++)
      {
        //adjust shifts with respect to the minimum shift due to truncation
        node->input_shifts[i] -= zeroLSBs;
      }
    }

    //Step 2: split output parts that belong to one of the inputs and assign it to the outputs

    //determine the difference between smallest shift (incl. truncation) of a non-negative input and second smallest shift (incl. truncation)
    int minShift=INT_MAX;
    int inputMinShift=-1;

    //check for negative inputs and ignore as they can't be forwarded to the output
    for(int i=0; i < node->input_shifts.size(); i++)
    {
      bool inputNegativeinAnyConf=false;
      if(!isConfigurableAddSub)
      {
        inputNegativeinAnyConf = node->input_is_negative[i]; //this is the simple case
      }
      else
      {
        //for configurable add/sub, check all configurations and ignore negative inputs
        for(int c=0; c < cnode->input_is_negative.size(); c++)
        {
          if(cnode->input_is_negative[c][i]) inputNegativeinAnyConf = true;
        }
      }
      if(!inputNegativeinAnyConf) //only consider non-negative inputs
      {
        if(node->input_shifts[i] < minShift)
        {
          minShift = node->input_shifts[i];
          inputMinShift = i;
        }
      }
    }
    int minShiftSecond=INT_MAX;;
    for(int i=0; i < node->input_shifts.size(); i++)
    {
      if((node->input_shifts[i] < minShiftSecond) && (i != inputMinShift))
        minShiftSecond = node->input_shifts[i];
    }
    int forwardedLSBs;
    if((minShift != INT_MAX) && (minShiftSecond != INT_MAX))
    {
      forwardedLSBs = minShiftSecond - minShift;
      forwardedLSBs = forwardedLSBs < 0 ? 0 : forwardedLSBs;
    }
    else
    {
      forwardedLSBs = 0;
    }

    if(forwardedLSBs > wIn)
      forwardedLSBs = wIn; //we can't forward more than we have

    if(forwardedLSBs > 0)
    {
      assert(inputMinShift != -1);
      REPORT(DETAIL,"Second smallest shift of non-negative input is " << minShiftSecond << ", hence " << forwardedLSBs << " LSBs can be copied to the result.");
      vhdl << tab << declare(signalNameOut + "_LSBs",forwardedLSBs) << " <= " << signalNameIn[inputMinShift] << (isTruncated ? "_trunc" : "") << range(forwardedLSBs - 1, 0) << ";" << endl;

      if(wAddIn[inputMinShift] > forwardedLSBs) //if this condition is true, there is nothing left to add (because of the shift)
      {
        vhdl << tab << declare(signalNameIn[inputMinShift] + "_MSBs", wAddIn[inputMinShift] - forwardedLSBs) << " <= " << signalNameIn[inputMinShift] << (isTruncated ? "_trunc" : "") << range(wAddIn[inputMinShift] - 1, forwardedLSBs) << ";" << endl;
      }
      else
      {
        REPORT(DETAIL,"Input " << inputMinShift << " does not contribute to the addition.");

        //the simple solution here is to add a 0 (unsigned) or the sign bit (signed case) (0 in unsigned case is later optimized by synthesis, potential of improvement for version using primitives!)
        string source;
        if(isSigned)
        {
          source = signalNameIn[inputMinShift] + range(wIn-1,wIn-1);
        }
        else
        {
          source = "\"0\""; //assign MSB to 0
        }

        vhdl << tab << declare(signalNameIn[inputMinShift] + "_MSBs", 1) << " <= " << source << ";" << endl;

      }

      for(int i=0; i < node->input_shifts.size(); i++)
      {
        //adjust shifts with respect to the forwarded LSBs
        if(i != inputMinShift)
          node->input_shifts[i] -= forwardedLSBs;

        //adjust word sizes
        if(i == inputMinShift)
          wAddIn[i]-= forwardedLSBs;
      }

    }

    //Step 3: shift input signals, and sign-extend inputs in signed case
    int wMaxInclShift=-1;
    //search for the maximum MSB position:
    for(int i=0; i < node->input_shifts.size(); i++)
    {
      if(node->input_shifts[i]+wAddIn[i] > wMaxInclShift)
      {
        wMaxInclShift = node->input_shifts[i] + wAddIn[i];
      }
    }
    REPORT(DEBUG,"Max. MSB position found at " << wMaxInclShift << ", sign extending signals");


    int wAddInMax=0;
    for(int i=0; i < node->inputs.size(); i++)
    {
      wAddInMax = wAddIn[i] > wAddInMax ? wAddIn[i] : wAddInMax; //store max input word size for later
    }

    int wAdd = wAddOut - forwardedLSBs - zeroLSBs;
    if(wAddInMax > wAdd) wAdd = wAddInMax; //a seldom case but may happen that input word size is larger than output word size

    int rightShift=0;
    for(int i=0; i < node->inputs.size(); i++)
    {
//      wAddIn[i]+= node->input_shifts[i];

      string conversionFunction;
      if(isSigned)
      {
        conversionFunction = "signed";
      }
      else
      {
        conversionFunction = "unsigned";
      }

      int leftShift=node->input_shifts[i];
      if(leftShift < 0)
      {
        //we have a right shift
        if(rightShift != 0)
        {
          if(rightShift != -leftShift)
          {
            THROWERROR("adder with different negative input shifts (right shifts) detected. This is not possible.");
          }
        }
        else
        {
          rightShift=-leftShift; //store right shift for later (after addition)
          wAdd += rightShift;    //increase adder word size by the right shift
          wAddOut += rightShift; //increase adder result by the right shift
        }
        leftShift=0; //reset shift to 0
      }

//      if(!((i == inputMinShift) && (wAddIn[inputMinShift] > forwardedLSBs))) //filter signal that does not contribute to the addition
      vhdl << tab << declare(signalNameIn[i] + "_shifted", wAdd) << " <= std_logic_vector(shift_left(resize(" << conversionFunction << "(" << signalNameIn[i] << ((forwardedLSBs > 0) && (i == inputMinShift) ? "_MSBs" : (isTruncated ? "_trunc" : "")) << ")," << wAdd << ")," << leftShift << "));" << endl;
    }

    //Step 4: Perform addition
//    if(getTarget()->plainVHDL())
    {
      vhdl << tab << declare(signalNameOut + "_MSBs",wAddOut - forwardedLSBs - zeroLSBs) << " <= ";
      if(noOfConfigurations > 1)
        vhdl << endl << tab << tab;
      string conversionFunction;

      if(isSigned || !nodeIsNormalized) //signed is necessary for nodes that could not be normalized
        conversionFunction = "signed";
      else
        conversionFunction = "unsigned";

      if(!isConfigurableAddSub)
      {
        //for the simple add/sub, perform the negation in the operation (usually more efficient for the tools)
        vhdl << "std_logic_vector(resize(";
        for(int i=0; i < node->inputs.size(); i++)
        {
          vhdl << (node->input_is_negative[i] ? "-" : (i == 0 ? "" : "+")) << conversionFunction << "(" << signalNameIn[i] + "_shifted" << ")";
        }
        vhdl << "," << wAddOut - forwardedLSBs - zeroLSBs << "));" << endl;
      }
      else
      {
        //for the configurable add/sub, the negation was already performed on the input signal
        for(int c = 0; c < cnode->input_is_negative.size(); c++)
        {
          vhdl << "std_logic_vector(";
          for(int i=0; i < node->inputs.size(); i++)
          {
            vhdl << (cnode->input_is_negative[c][i] ? "-" : (i == 0 ? "" : "+")) << "resize(" << conversionFunction << "(" << signalNameIn[i] + "_shifted" << ")" << "," << wAddOut - forwardedLSBs - zeroLSBs << ")";
          }
          vhdl << ")";
          if(c != cnode->input_is_negative.size()-1)
          {
            vhdl << " when " << generateSelectName() << "=\"" << dec2binstr(c, wSel) << "\"";
            vhdl << " else " << endl << tab << tab;
          }
          else
          {
            vhdl << ";" << endl;
          }
        }
      }
    }
/*
 *  (otherwise autotest will not work ...)
 *
    else
    {
      THROWERROR("Target specific optimization not complete yet, use plainVHDL=1 instead");
    }
*/
    //Step 5: Merge results, perform right shift if necessary
    vhdl << tab << signalNameOut << " <= ";
    vhdl << signalNameOut << "_MSBs";
    if(rightShift > 0)
    {
      vhdl << range(wAddOut-1,rightShift);
    }
    if(forwardedLSBs > 0)
    {
       vhdl << " & " << signalNameOut << "_LSBs";
    }
    if(zeroLSBs > 0)
    {
      vhdl << " & \"";
      for(int i=0; i < zeroLSBs; i++)
      {
        vhdl << "0";
      }
      vhdl << "\"";
    }
    vhdl << ";" << endl;




    //determine flags for the mode of the Adder / Sub:
/*
    int nb_inputs = node->inputs.size();
    int flag = (nb_inputs == 3) ? IntAddSub::TERNARY : 0;
    if (node->input_is_negative[0]) {
      flag |= IntAddSub::SUB_LEFT;
    }
    if (node->input_is_negative[1]) {
      flag |= (nb_inputs == 3) ? IntAddSub::SUB_MID : IntAddSub::SUB_RIGHT;
    }

    if (nb_inputs > 2 && node->input_is_negative[2]) {
      flag |= IntAddSub::SUB_RIGHT;
    }
*/


/*
 TODO: integrate this!

    bool isSigned=false;
    bool isTernary=false;
    bool xNegative=false;
    bool yNegative=false;
    bool zNegative=false;
    bool xConfigurable=false;
    bool yConfigurable=false;
    bool zConfigurable=false;



    IntAddSub* add = new IntAddSub(
      this,
      getTarget(),
      wAdd,isSigned, isTernary, xNegative, yNegative, zNegative, xConfigurable, yConfigurable, zConfigurable
    );

    addSubComponent(add);
*/

  /*

    vector<string> operandNames(nb_inputs);
    for (size_t i = 0 ; i < nb_inputs ; ++i)
    {
      string operandName = generateSignalName(node->inputs[i]->output_factor) + "_";
      operandNames.push_back();
      declare(0., signal_name, adder_word_size);
    }

    for (size_t i = 0 ; i < nb_inputs ; ++i)
    {
      inPortMap(add->getInputName(i), operandNames[i]);
    }
    base_op->outPortMap(add->getOutputName(), msb_signame);

    base_op->vhdl << base_op->instance(add,"generic_add_sub_"+outputSignalName);

    string leftshiftedoutput = outputSignalName + "_tmp";

//  base_op->vhdl << "\t" << declare(leftshiftedoutput, wordsize + finalRightShifts) << " <= " << msb_signame;
    base_op->vhdl << "\t" << declare(leftshiftedoutput, wordsize) << " <= " << msb_signame;
*/

  }


  void IntConstMultShiftAdd::generateMuxNode(PAGSuite::mux_node_t* node)
  {
    //some detailed output about what is computed:
    if(is_log_lvl_enabled(DETAIL)) {
      cerr << std::filesystem::path{__FILE__}.filename() << ": ";
      cerr << "processing MUX computing:" << endl;

      for(int i=0; i < node->inputs.size(); i++)
      {
        adder_graph_base_node_t *input_node = node->inputs[i];
        if(input_node != nullptr) //don't care inputs are defined to be NULL
        {
          cerr << std::filesystem::path{__FILE__}.filename() << ": ";
          cerr << "[" << node->output_factor << "] (stage " << node->stage << ")" << " = [" << input_node->output_factor << "] (stage " << input_node->stage << ")" << " << " << ((mux_node_t *) node)->input_shifts[i] << " for configuration " << i << endl;
        }
      }
    }

    int wMUX=computeWordSize(node->output_factor, wIn);

    vhdl << tab << "with " << generateSelectName() << " select" << endl;
    vhdl << tab << declare(generateSignalName(node->output_factor, node->stage),wMUX) << " <= " << endl << tab << tab << tab;;
    string signed_str = isSigned ? "signed" : "unsigned";
    for(int i=0; i < node->inputs.size(); i++)
    {
      adder_graph_base_node_t *input_node = node->inputs[i];

      if(input_node != nullptr) //don't care inputs are defined to be NULL
      {
        vhdl << "std_logic_vector(" << signed_str << "(shift_left(resize(" << signed_str << "(" << generateSignalName(input_node->output_factor, input_node->stage) << ")," << wMUX << ")," << ((mux_node_t *) node)->input_shifts[i] << ")))";
      }
      else
      {
        vhdl << zg(wMUX); //output zero for don't care
      }
      vhdl << " when ";
      if(i < ((mux_node_t *) node)->inputs.size() - 1)
        vhdl << "\"" << dec2binstr(i, wSel) << "\"," << endl << tab << tab << tab;
      else
        vhdl << "others;" << endl;


    }
  }



  void IntConstMultShiftAdd::emulate(TestCase * tc)
	{
		vector<mpz_class> inputVector(noOfInputs);

    mpz_class msbp1 = (mpz_class(1) << (wIn));
    mpz_class msb = (mpz_class(1) << (wIn - 1));

		for(int i=0 ; i < noOfInputs ; i++)
		{
			inputVector[i] = tc->getInputValue("X" + to_string(i));

      if(isSigned)
      {
        if (inputVector[i] >= msb)
          inputVector[i] = inputVector[i] - msbp1;
      }

    }

//#define DEBUG_OUT
#ifdef DEBUG_OUT
    cerr << "Input vector(s): ";
		for(int i=0 ; i < noOfInputs ; i++)
      cerr << inputVector[i] << " ";
#endif// DEBUG_OUT

    mpz_class conf_mpz;
    if(noOfConfigurations > 1)
    {
      conf_mpz = tc->getInputValue(generateSelectName());
    }
    else
    {
      conf_mpz = 0;
    }
    int conf = conf_mpz.get_ui();

    //check the case that the configuration does not exist
    if(conf > noOfConfigurations-1)
    {
      conf_mpz = conf % noOfConfigurations; //set it to a valid configuration
      conf = conf_mpz.get_ui();
      tc->setInputValue(generateSelectName(), conf_mpz);
    }

#ifdef DEBUG_OUT
    cerr << " | output value(s): ";
#endif// DEBUG_OUT
    for(adder_graph_base_node_t* node : adder_graph.nodes_list)
    {
      if(is_a<output_node_t>(*node))
      {
        signed long int outputValue=0;
    		for(int i=0 ; i < noOfInputs ; i++)
        {
          outputValue += inputVector[i].get_si() * node->output_factor[conf][i];
        }
        string outputName = "R_" + factorToString(node->output_factor);
        mpz_class outputValue_mpz = outputValue;
        tc->addExpectedOutput(outputName, outputValue_mpz, isSigned);
        if(epsilon > 0)
        {
          //Probably not the most efficient way for large epsilons...
          for(int e=1; e <= epsilon; e++)
          {
            tc->addExpectedOutput(outputName, outputValue+e);
            tc->addExpectedOutput(outputName, outputValue-e);
          }
        }

#ifdef DEBUG_OUT
        cerr << outputValue << " ";
#endif// DEBUG_OUT

        // ... to be continued ...
      }
    }
#ifdef DEBUG_OUT
    cerr << endl;
#endif// DEBUG_OUT


	}

	void IntConstMultShiftAdd::buildStandardTestCases(TestCaseList * tcl)
	{
		TestCase* tc;
    if(isSigned)
    {
      for(int c=0; c < noOfConfigurations; c++)
      {
        tc = new TestCase (this);
        tc->addComment("Test ZERO");
        if( noOfConfigurations > 1 )
        {
          tc->addInput(generateSelectName(), c);
        }
        for(int i=0; i < noOfInputs; i++)
        {
          tc->addInput("X" + to_string(i),0 );
        }
        emulate(tc);
        tcl->add(tc);
      }
    }

		for(int c=0; c < noOfConfigurations; c++)
		{
			tc = new TestCase (this);
			tc->addComment("Test ONE");
			if( noOfConfigurations > 1 )
			{
				tc->addInput(generateSelectName(), c);
			}
      for(int i=0; i < noOfInputs; i++)
      {
        tc->addInput("X" + to_string(i),1 );
			}
			emulate(tc);
			tcl->add(tc);
		}

    long int min_val, max_val;
    if(isSigned)
    {
      min_val = -1 * (1 << (wIn-1));
      max_val = (1 << (wIn-1)) -1;
    }
    else
    {
      min_val = 0;
      max_val = (1 << wIn) -1;
    }

		for(int c=0; c < noOfConfigurations; c++)
		{
			tc = new TestCase (this);
			tc->addComment("Test min value");
			if( noOfConfigurations > 1 )
			{
				tc->addInput(generateSelectName(), c);
			}
      for(int i=0; i < noOfInputs; i++)
      {
        tc->addInput("X" + to_string(i),min_val );
			}
			emulate(tc);
			tcl->add(tc);
		}

		for(int c=0; c < noOfConfigurations; c++)
		{
			tc = new TestCase (this);
			tc->addComment("Test max value");
			if( noOfConfigurations > 1 )
			{
				tc->addInput(generateSelectName(), c);
			}
      for(int i=0; i < noOfInputs; i++)
      {
        tc->addInput("X" + to_string(i),max_val );
			}
			emulate(tc);
			tcl->add(tc);
		}
	}


	TestList IntConstMultShiftAdd::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;

		list<string> graphsUnsigned;
		list<string> graphsSigned;

    //adder graphs having only unsigned outputs (can be evaluated on both signed & unsigned):

		//simplest adder graph possible, multiply by 1:
		graphsUnsigned.push_back("{{'O',[1],1,[1],0,0}}");

    //Simple SCM by 7, obtained from rpag 7:
    graphsUnsigned.push_back("{{'A',[7],1,[1],0,3,[-1],0,0},{'O',[7],1,[7],1,0}}");

    //SCM by 7, but swapped inputs such that the first input is negative (check for problems with unsigned input):
    graphsUnsigned.push_back("{{'A',[7],1,[-1],0,0,[1],0,3},{'O',[7],1,[7],1,0}}");

    //Simple SCM with negative result -7:
    graphsSigned.push_back("{{'A',[-7],1,[1],0,0,[-1],0,3},{'O',[-7],1,[-7],1,0}}");

    //Simple SCM with negative result -9 and both inputs negative:
    graphsSigned.push_back("{{'A',[-9],1,[-1],0,0,[-1],0,3},{'O',[-9],1,[-9],1,0}}");

		//SCM by 123, obtained from rpag 123:
		graphsUnsigned.push_back("{{'R',[1],1,[1],0},{'A',[5],1,[1],0,0,[1],0,2},{'A',[123],2,[1],1,7,[-5],1,0},{'O',[123],2,[123],2,0}}");

		//MCM by 123, 321, obtained from rpag 123 321:
		graphsUnsigned.push_back("{{'R',[1],1,[1],0},{'A',[5],1,[1],0,0,[1],0,2},{'A',[123],2,[1],1,7,[-5],1,0},{'A',[321],2,[1],1,0,[5],1,6},{'O',[123],2,[123],2,0},{'O',[321],2,[321],2,0}}");

		//SCM by 100000000 using ternary adders, obtained from rpag --ternary_adders 100000000:
		graphsUnsigned.push_back("{{'A',[191],1,[1],0,6,[1],0,7,[-1],0,0},{'A',[543],1,[1],0,5,[1],0,9,[-1],0,0},{'A',[390625],2,[191],1,11,[-543],1,0},{'O',[100000000],2,[390625],2,8}}");

		//simple SCM by 11 using ternary adders for which one bit can be forwarded
		graphsUnsigned.push_back("{{'A',[11],1,[1],0,0,[1],0,1,[1],0,3},{'O',[11],1,[11],1,0}}");

		//MCM by 123, 321 using ternary adders, obtained from rpag --ternary_adders 123 321:
		graphsUnsigned.push_back("{{'A',[123],1,[1],0,7,[-1],0,2,[-1],0,0},{'A',[321],1,[1],0,6,[1],0,8,[1],0,0},{'O',[123],1,[123],1,0},{'O',[321],1,[321],1,0}}");

		//MCM by 11280171, 13342037 using ternary adders, obtained from rpag --ternary_adders 11280171 13342037:
		graphsUnsigned.push_back("{{'A',[21],1,[1],0,2,[1],0,4,[1],0,0},{'A',[8065],1,[1],0,13,[-1],0,7,[1],0,0},{'A',[10941],2,[21],1,3,[21],1,9,[21],1,0},{'A',[104833],2,[21],1,9,[21],1,12,[8065],1,0},{'A',[11280171],3,[10941],2,3,[10941],2,10,[-10941],2,0},{'A',[13342037],3,[10941],2,0,[-10941],2,3,[104833],2,7},{'O',[11280171],3,[11280171],3,0},{'O',[13342037],3,[13342037],3,0}}"); //

		//simple CMM having only positive values, obtained from rpag --cmm 3,7 5,9:
		graphsUnsigned.push_back("{{'A',[1,1],1,[0,1],0,0,[1,0],0,0},{'A',[1,2],1,[0,1],0,1,[1,0],0,0},{'A',[3,7],2,[1,2],1,2,[-1,-1],1,0},{'A',[5,9],2,[1,1],1,0,[1,2],1,2},{'O',[3,7],2,[3,7],2,0},{'O',[5,9],2,[5,9],2,0}}");

		//simple SCM including right shifts:
		graphsUnsigned.push_back("{{'A',[3],1,[1],0,1,[1],0,0},{'A',[7],1,[1],0,3,[-1],0,0},{'A',[5],2,[3],1,-1,[7],1,-1},{'O',[5],2,[5],2,0}}");

		//More comples MCM's including right shifts:
		graphsUnsigned.push_back("{{'R',[1],1,[1],0},{'A',[31],1,[1],0,5,[-1],0,0},{'A',[511],1,[1],0,9,[-1],0,0},{'A',[2049],1,[1],0,0,[1],0,11},{'A',[123],2,[31],1,2,[-1],1,0},{'A',[6127],2,[511],1,4,[-2049],1,0},{'A',[1049119],2,[31],1,0,[2049],1,9},{'A',[5079583],3,[123],2,15,[1049119],2,0},{'A',[6274171],3,[123],2,0,[6127],2,10},{'A',[5676877],4,[5079583],3,-1,[6274171],3,-1},{'A',[25397915],4,[5079583],3,0,[5079583],3,2},{'O',[50795830],4,[25397915],4,1},{'O',[90830032],4,[5676877],4,4}}");
		graphsUnsigned.push_back("{{'O',[6],3,[3],2,1},{'O',[10],3,[5],1,1},{'A',[3],2,[1],0,-1,[5],1,-1},{'A',[5],1,[1],0,2,[1],0,0}}"); //

		//SCM by 1025 which can be implemented without addition when wIn<10 bits:
		graphsUnsigned.push_back("{{'A',[1025],1,[1],0,0,[1],0,10},{'O',[1025],1,[1025],1,0}}");

		//SCM by 3073 using one ternary adder where one argument can be forwarded to the output when wIn<10 bits:
		graphsUnsigned.push_back("{{'A',[3073],1,[1],0,0,[1],0,10,[1],0,11},{'O',[3073],1,[3073],1,0}}");

    //artificial SCM using arbitrary stages
		graphsUnsigned.push_back("{{'R',[1],1,[1],0},{'A',[3],3,[1],0,0,[1],1,1},{'O',[6],5,[3],3,1}}");


    //adder graph having negative outputs (can be only evaluated for signed):
		graphsSigned.push_back("{{'A',[-7],1,[-1],0,3,[1],0,0},{'O',[-7],1,[-7],1,0}}");

    //adder graph having a non-common order of nodes (output first):
		graphsSigned.push_back("{{'O',[-7],1,[-7],1,0},{'A',[-7],1,[-1],0,3,[1],0,0}}");

		//CMM of 123*x1+321*x2 345*x1-543*x2, obtained from rpag --cmm 123,321 345,-543:
		graphsSigned.push_back("{{'A',[0,5],1,[0,1],0,0,[0,1],0,2},{'A',[0,17],1,[0,1],0,0,[0,1],0,4},{'A',[5,0],1,[1,0],0,0,[1,0],0,2},{'A',[128,1],1,[0,1],0,0,[1,0],0,7},{'A',[257,0],1,[1,0],0,0,[1,0],0,8},{'R',[0,5],2,[0,5],1},{'A',[5,68],2,[0,17],1,2,[5,0],1,0},{'A',[123,1],2,[128,1],1,0,[-5,0],1,0},{'A',[385,1],2,[128,1],1,0,[257,0],1,0},{'A',[123,321],3,[0,5],2,6,[123,1],2,0},{'A',[345,-543],3,[385,1],2,0,[-5,-68],2,3},{'O',[123,321],3,[123,321],3,0},{'O',[345,-543],3,[345,-543],3,0}}");

		//CMM of 123*x1+321*x2 345*x1-543*x2 using ternary adders, obtained from rpag --ternary_adders --cmm 123,321 345,-543:
		graphsSigned.push_back("{{'A',[1,-4],1,[1,0],0,0,[0,-1],0,2},{'A',[2,5],1,[0,1],0,0,[0,1],0,2,[1,0],0,1},{'A',[5,-1],1,[1,0],0,2,[0,-1],0,0,[1,0],0,0},{'A',[7,-1],1,[1,0],0,3,[0,-1],0,0,[-1,0],0,0},{'A',[123,321],2,[2,5],1,6,[-5,1],1,0},{'A',[345,-543],2,[1,-4],1,7,[7,-1],1,5,[-7,1],1,0},{'O',[123,321],2,[123,321],2,0},{'O',[345,-543],2,[345,-543],2,0}}"); //

    /*RSCM of 5;9 using one adder and a MUX
     * obtained from:
     * ./rpag 5 9
     * ./pag_split "{{'A',[5],1,[1],0,0,[1],0,2},{'A',[9],1,[1],0,0,[1],0,3},{'O',[5],1,[5],1,0},{'O',[9],1,[9],1,0}}" "5;9" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
     */
    graphsUnsigned.push_back("{{'R',[1;1],1,[1;1],0},{'M',[1;2],1,[1;1],0,[0;1]},{'A',[5;9],2,[1;1],1,0,[1;2],1,2},{'O',[5;9],2,[5;9],2}}");

    /*RSCM of 7;9 using one adder/subtractor
     * obtained from:
     * ./rpag 7 9
     * ./pag_split "{{'A',[7],1,[1],0,3,[-1],0,0},{'A',[9],1,[1],0,0,[1],0,3},{'O',[7],1,[7],1,0},{'O',[9],1,[9],1,0}}" "7;9" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
     */
    graphsUnsigned.push_back("{{'A',[7;9],1,[1;1],0,3,[-1;1],0,0},{'O',[7;9],1,[7;9],1}}");


    /*RSCM of 5;7 using one adder/subtractor and a MUX
     * obtained from:
     * ./rpag 5 7
     * ./pag_split "{{'A',[5],1,[1],0,0,[1],0,2},{'A',[7],1,[1],0,3,[-1],0,0},{'O',[5],1,[5],1,0},{'O',[7],1,[7],1,0}}" "5;7" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
     */
    graphsUnsigned.push_back("{{'R',[1;1],1,[1;1],0},{'M',[1;2],1,[1;1],0,[0;1]},{'A',[5;7],2,[1;-1],1,0,[1;2],1,2},{'O',[5;7],2,[5;7],2}}");

    /*RSCM of 71;97
     * obtained from:
     * ./rpag --cost_model=hl_min_ad 127 7 97 71 (and removing some output/register nodes)
     * ./pag_split "{{'R',[1],1,[1],0},{'A',[7],1,[1],0,3,[-1],0,0},{'A',[127],1,[1],0,7,[-1],0,0},{'A',[71],2,[1],1,6,[7],1,0},{'A',[97],2,[7],1,5,[-127],1,0},{'O',[71],2,[71],2,0},{'O',[97],2,[97],2,0}}" "71;97" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
     */
    graphsUnsigned.push_back("{{'M',[1;8],1,[1;1],0,[0;3]},{'M',[0;1],1,[1;1],0,[NaN;0]},{'A',[1;7],2,[1;8],1,0,[0;-1],1,0},{'M',[1;16],1,[1;1],0,[0;4]},{'R',[1;1],1,[1;1],0},{'A',[7;127],2,[1;16],1,3,[-1;-1],1,0},{'M',[2;7],3,[1;7],2,[1;0]},{'R',[7;127],3,[7;127],2},{'A',[71;97],4,[2;7],3,5,[7;-127],3,0},{'O',[71;97],4,[71;97],4}}");

    /*RSCM of -57;97 having negative and positive outputs and intermediate values with larger wordsize than the output (hand crafted from the 71;97 RSCM)
     */
    graphsSigned.push_back("{{'M',[1;8],1,[1;1],0,[0;3]},{'M',[0;1],1,[1;1],0,[NaN;0]},{'A',[1;7],2,[1;8],1,0,[0;-1],1,0},{'M',[1;16],1,[1;1],0,[0;4]},{'R',[1;1],1,[1;1],0},{'A',[7;127],2,[1;16],1,3,[-1;-1],1,0},{'M',[2;7],3,[1;7],2,[1;0]},{'R',[7;127],3,[7;127],2},{'A',[-57;97],4,[-2;7],3,5,[7;-127],3,0},{'O',[-57;97],4,[-57;97],4}}");

    /*RSCM of 1;2;3;4;5
     * obtained from:
     * ./rpag 1 2 3 4 5
     * ./pag_split "{{'R',[1],1,[1],0},{'A',[3],1,[1],0,0,[1],0,1},{'A',[5],1,[1],0,0,[1],0,2},{'O',[1],1,[1],1,0},{'O',[2],1,[1],1,1},{'O',[3],1,[3],1,0},{'O',[4],1,[1],1,2},{'O',[5],1,[5],1,0}}" "1;2;3;4;5" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
     */
    graphsUnsigned.push_back("{{'M',[1;2;2;4;4],1,[1;1;1;1;1],0,[0;1;1;2;2]},{'M',[0;0;1;0;1],1,[1;1;1;1;1],0,[NaN;NaN;0;NaN;0]},{'A',[1;2;3;4;5],2,[1;2;2;4;4],1,0,[0;0;1;0;1],1,0},{'O',[1;2;3;4;5],2,[1;2;3;4;5],2}}");


    /*RMCM of 123;543;412 345;321;654
     * obtained from:
     * ./rpag 123 345 543 654 321 412
     * ./pag_split "{{'R',[1],1,[1],0},{'A',[17],1,[1],0,0,[1],0,4},{'R',[1],2,[1],1},{'A',[15],2,[1],1,4,[-1],1,0},{'A',[69],2,[1],1,0,[17],1,2},{'A',[153],2,[17],1,0,[17],1,3},{'A',[103],3,[1],2,8,[-153],2,0},{'A',[123],3,[69],2,1,[-15],2,0},{'A',[321],3,[15],2,0,[153],2,1},{'A',[327],3,[15],2,5,[-153],2,0},{'A',[345],3,[69],2,0,[69],2,2},{'A',[543],3,[153],2,2,[-69],2,0},{'O',[123],3,[123],3,0},{'O',[321],3,[321],3,0},{'O',[345],3,[345],3,0},{'O',[412],3,[103],3,2},{'O',[543],3,[543],3,0},{'O',[654],3,[327],3,1}}" "123;543;412 345;321;654" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
     */
    graphsUnsigned.push_back("{{'A',[17;17;17],1,[1;1;1],0,0,[1;1;1],0,4},{'R',[1;1;1],1,[1;1;1],0},{'R',[1;1;1],2,[1;1;1],1},{'A',[15;15;15],3,[1;1;1],2,4,[-1;-1;-1],2,0},{'M',[17;17;0],2,[17;17;17],1,[0;0;NaN]},{'A',[69;69;1],3,[1;1;1],2,0,[17;17;0],2,2},{'R',[NaN;17;17],2,[17;17;17],1},{'A',[NaN;153;153],3,[NaN;17;17],2,0,[NaN;17;17],2,3},{'M',[69;306;306],4,[69;69;1],3,[0;NaN;NaN],[NaN;153;153],3,[NaN;1;1]},{'M',[15;69;1024],4,[15;15;15],3,[0;NaN;NaN],[69;69;1],3,[NaN;0;10]},{'A',[123;543;412],5,[69;306;-306],4,1,[-15;-69;1024],4,0},{'M',[69;15;960],4,[69;69;1],3,[0;NaN;NaN],[15;15;15],3,[NaN;0;6]},{'M',[138;153;153],4,[69;69;1],3,[1;NaN;NaN],[NaN;153;153],3,[NaN;0;0]},{'A',[345;321;654],5,[69;15;960],4,0,[138;153;-153],4,1},{'O',[123;543;412],5,[123;543;412],5},{'O',[345;321;654],5,[345;321;654],5}}");

    /*RCMM with two inputs and two configurations
     * obtained from:
     * ./rpag --cmm 123+321 345+543 567+765 789+987 --file_output
     * ./pag_split "{{'A',[1,-64],1,[1,0],0,0,[0,-1],0,6},{'A',[1,-1],1,[1,0],0,0,[0,-1],0,0},{'A',[3,0],1,[1,0],0,0,[1,0],0,1},{'R',[1,-1],2,[1,-1],1},{'A',[47,64],2,[3,0],1,4,[-1,64],1,0},{'A',[33,-33],3,[1,-1],2,0,[1,-1],2,5},{'A',[189,255],3,[1,-1],2,0,[47,64],2,2},{'A',[123,321],4,[189,255],3,0,[-33,33],3,1},{'A',[345,543],4,[189,255],3,1,[-33,33],3,0},{'A',[567,765],4,[189,255],3,0,[189,255],3,1},{'A',[789,987],4,[33,-33],3,0,[189,255],3,2},{'O',[123,321],4,[123,321],4,0},{'O',[345,543],4,[345,543],4,0},{'O',[567,765],4,[567,765],4,0},{'O',[789,987],4,[789,987],4,0}}" "123,321;345,543 567,765;789,987" --pag_fusion_input
     * ./pag_fusion --if pag_fusion_input.txt
    */
    graphsSigned.push_back("{{'A',[1,-64;1,-64],1,[1,0;1,0],0,0,[0,-1;0,-1],0,6},{'A',[1,-1;1,-1],1,[1,0;1,0],0,0,[0,-1;0,-1],0,0},{'A',[3,0;3,0],1,[1,0;1,0],0,0,[1,0;1,0],0,1},{'R',[1,-1;1,-1],2,[1,-1;1,-1],1},{'A',[47,64;47,64],2,[3,0;3,0],1,4,[-1,64;-1,64],1,0},{'R',[1,-1;1,-1],3,[1,-1;1,-1],2},{'M',[8,-8;47,64],3,[1,-1;1,-1],2,[3;NaN],[47,64;47,64],2,[NaN;0]},{'A',[33,-33;189,255],4,[1,-1;1,-1],3,0,[8,-8;47,64],3,2},{'M',[47,64;8,-8],3,[47,64;47,64],2,[0;NaN],[1,-1;1,-1],2,[NaN;3]},{'A',[189,255;33,-33],4,[1,-1;1,-1],3,0,[47,64;8,-8],3,2},{'R',[189,255;33,-33],5,[189,255;33,-33],4},{'R',[33,-33;189,255],5,[33,-33;189,255],4},{'A',[123,321;345,543],6,[189,255;-33,33],5,0,[-33,33;189,255],5,1},{'M',[189,255;378,510],5,[189,255;33,-33],4,[0;NaN],[33,-33;189,255],4,[NaN;1]},{'A',[567,765;789,987],6,[189,255;33,-33],5,0,[189,255;378,510],5,1},{'O',[123,321;345,543],6,[123,321;345,543],6},{'O',[567,765;789,987],6,[567,765;789,987],6}}");

#ifdef RMCM_SUPPORT


#endif // RMCM_SUPPORT

//  graphs.push_back(""); //
//  graphs.push_back(""); //

    if(testLevel >= TestLevel::QUICK)
    {
      for(auto g : graphsUnsigned)
      {
        paramList.push_back(make_pair("wIn", "10"));
        paramList.push_back(make_pair("graph", g));
        paramList.push_back(make_pair("signed", "false"));
        testStateList.push_back(paramList);
        paramList.clear();
      }
      for(auto g : graphsSigned)
      {
        paramList.push_back(make_pair("wIn", "10"));
        paramList.push_back(make_pair("graph", g));
        paramList.push_back(make_pair("signed", "true"));
        testStateList.push_back(paramList);
        paramList.clear();
      }
    }
    else
    {
      for(int wIn=10; wIn<=32; wIn+=22) // test various input widths
      {
        for(auto g : graphsUnsigned)
        {
          paramList.push_back(make_pair("wIn", to_string(wIn)));
          paramList.push_back(make_pair("signed", "false"));
          paramList.push_back(make_pair("graph", g));
          testStateList.push_back(paramList);
          paramList.clear();
        }
        for(auto g : graphsUnsigned)
        {
          paramList.push_back(make_pair("wIn", to_string(wIn)));
          paramList.push_back(make_pair("signed", "true"));
          paramList.push_back(make_pair("graph", g));
          testStateList.push_back(paramList);
          paramList.clear();
        }
        for(auto g : graphsSigned)
        {
          paramList.push_back(make_pair("wIn", "10"));
          paramList.push_back(make_pair("graph", g));
          paramList.push_back(make_pair("signed", "true"));
          testStateList.push_back(paramList);
          paramList.clear();
        }
      }
    }

		return testStateList;
	}

	string IntConstMultShiftAdd::generateSignalName(std::vector<std::vector<int64_t> > factor, int stage)
	{
		stringstream signalName;
    signalName << "x_" << factorToString(factor) << "_s" << stage;

		return signalName.str();
	}

  string IntConstMultShiftAdd::dec2binstr(int x, int wordsize)
  {
    string s;

    if(wordsize == -1)
    {
      wordsize = (x < 2) ? 1 : ceil(log2(x+1));
    }
    for(int i=wordsize-1; i >= 0; i--)
    {
      if((1 << i) & x)
        s += "1";
      else
        s += "0";
    }

    return s;
  }

  string IntConstMultShiftAdd::factorToString(std::vector<std::vector<int64_t> > factor)
  {
    stringstream signalName;
    for(int c=0; c < factor.size(); c++ ) //loop over configurations
    {
      signalName << "c";
      for(int i=0; i < factor[c].size(); i++) // loop over inputs
      {
        if(i > 0) signalName << "_c";
        if(factor[c][i] != DONT_CARE )
        {
          if(factor[c][i] < 0 )
          {
            signalName << "m";
          }
          signalName << abs( factor[c][i] );
        }
        else
          signalName << "d";
      }
    }
    return signalName.str();
  }

  /*
  int flopoco::IntConstMultShiftAdd::computeWordSize(std::vector<std::vector<int64_t> > factor)
  {
    int no_of_extra_bits=-1;
    for(int c=0; c < factor.size(); c++ ) //loop over configurations
    {
      int64_t sum=0;
      for (int i = 0; i < factor[c].size(); i++) // loop over inputs
      {
        sum += factor[c][i];
      }
      no_of_extra_bits = max(no_of_extra_bits,log2c_64(sum));
    }
    return no_of_extra_bits;
  }
*/
	OperatorPtr flopoco::IntConstMultShiftAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
	{
		if (target->getVendor() != "Xilinx")
			throw std::runtime_error("Can't build xilinx primitive on non xilinx target");

		int wIn, sync_every = 0;
		std::string adder_graph, truncations;
    bool isSigned;
		int epsilon;

		ui.parseInt(args, "wIn", &wIn);
		ui.parseString(args, "graph", &adder_graph);
    ui.parseBoolean(args, "signed", &isSigned);
		ui.parseString( args, "truncations", &truncations);
		ui.parseInt(args, "epsilon", &epsilon);

		if (truncations == "\"\"") {
			truncations = "";
		}

		return new IntConstMultShiftAdd(parentOp, target, wIn, adder_graph, isSigned, epsilon, truncations);
	}


}//namespace


namespace flopoco {
	template <>
	const OperatorDescription<IntConstMultShiftAdd> op_descriptor<IntConstMultShiftAdd> {
	    "IntConstMultShiftAdd", // name
	    "A component for building constant multipliers based on "
	    "pipelined adder graphs (PAGs).", // description, string
	    "ConstMultDiv", // category, from the list defined in UserInterface.cpp
	    "",
	    "wIn(int): Wordsize of pag inputs; \
       graph(string): Realization string of the adder graph; \
       signed(bool)=true: signedness of input and output; \
       epsilon(int)=0: Allowable error for truncated constant multipliers; \
       truncations(string)=\"\": provides the truncations for intermediate values (format: const1,stage:trunc_input_0,trunc_input_1,...|const2,stage:trunc_input_0,trunc_input_1,...)",""};
       // (format const1,stage:trunc_input_0,trunc_input_1,...;const2,stage:trunc_input_0,trunc_input_1,...;...)
}//namespace

#endif // HAVE_PAGLIB
