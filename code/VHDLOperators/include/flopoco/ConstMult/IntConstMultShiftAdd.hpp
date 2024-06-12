#ifndef  IntConstMultShiftAdd_HPP
#define IntConstMultShiftAdd_HPP

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/UserInterface.hpp"

#if defined(HAVE_PAGLIB)
//&& defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#include "flopoco/utils.hpp"
#include "pagsuite/adder_graph.h"

using namespace std;

namespace flopoco
{
  class IntConstMultShiftAdd : public Operator
  {
  public:
//    static ostream nostream;
//    int noOfPipelineStages;

    IntConstMultShiftAdd(
      Operator *parentOp,
      Target *target,
      int wIn_,
      string graphStr,
      bool isSigned = true,
      int errorBudget_ = 0,
      string truncations = ""
    );

    ~IntConstMultShiftAdd(){}

    void emulate(TestCase *tc);

    void buildStandardTestCases(TestCaseList *tcl);

    static TestList unitTest(int testLevel);

//    std::list<std::vector<std::vector<int64_t> > > output_factors;
/*
    struct output_signal_info
    {
      string signal_name;
      vector<vector<int64_t> > output_factors;
      int wordsize;
    };

    list<output_signal_info> &GetOutputList();
*/
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui);

  protected:
    int wIn;
    bool isSigned;
    double errorBudget;
    int wSel;

    bool isTruncated; //if true, the operator is truncated truncated
    map<pair<mpz_class, int>, vector<int> > wordSizeMap; //stores the truncations of inputs; key is a pair of constant and stage, value is a vector of the number of truncated bits

//    bool RPAGused;
//    int emu_conf;

    void ProcessIntConstMultShiftAdd(Target* target, string graphStr, string truncations="", int epsilon=0);

    void parseTruncation(string truncationList);
    void parseTruncationRecord(string record);

    void generateAdderGraph(PAGSuite::adder_graph_t* adder_graph);
    void generateInputNode(PAGSuite::input_node_t* node);
    void generateOutputNode(PAGSuite::output_node_t* node);
    void generateZeroNode(PAGSuite::zero_node_t* node);
    void generateRegisterNode(PAGSuite::register_node_t* node);
    void generateMuxNode(PAGSuite::mux_node_t* node);
    void generateConfAdderSubtractorNode(PAGSuite::conf_adder_subtractor_node_t* node);
    void generateAdderSubtractorNode(PAGSuite::adder_subtractor_node_t* node);

    string generateSignalName(std::vector<std::vector<int64_t> > factor, int stage);
    string generateSelectName() { return "SEL"; };
    string factorToString(std::vector<std::vector<int64_t> > factor);
    static string dec2binstr(int x, int wordsize=-1); //wordsize=-1 means chose minimum word size
    //    int computeWordSize(std::vector<std::vector<int64_t> > factor);

    vector<vector<int64_t> > output_coefficients;
    PAGSuite::adder_graph_t adder_graph;
//    list<string> input_signals;

//    list<output_signal_info> output_signals;

    list<mpz_class> output_constants;

    int noOfInputs=-1;
    int noOfOutputs=-1;
    int noOfConfigurations=-1;
  };
}//namespace

#else
namespace flopoco {
    class IntConstMultShiftAdd : public Operator {
    public:
        static void registerFactory();
    };
}
#endif // defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#endif
