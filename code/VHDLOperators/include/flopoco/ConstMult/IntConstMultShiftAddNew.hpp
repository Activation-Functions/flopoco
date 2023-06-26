#ifndef  IntConstMultShiftAddNew_HPP
#define IntConstMultShiftAddNew_HPP

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
  class IntConstMultShiftAddNew : public Operator
  {
  public:
    static ostream nostream;
    int noOfPipelineStages;

    IntConstMultShiftAddNew(
      Operator *parentOp,
      Target *target,
      int wIn_,
      string adder_graph_str,
      bool isSigned = true,
      int epsilon_ = 0,
      string truncations = ""
    );

    ~IntConstMultShiftAddNew(){}

    void emulate(TestCase *tc);

    void buildStandardTestCases(TestCaseList *tcl);

    static TestList unitTest(int testLevel);

    struct output_signal_info
    {
      string signal_name;
      vector<vector<int64_t> > output_factors;
      int wordsize;
    };

    list<output_signal_info> &GetOutputList();

    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui);

  protected:
    int wIn;
    bool isSigned;
    double epsilon;

    bool RPAGused;
    int emu_conf;

    void parseTruncation(map<pair<mpz_class, int>, vector<int> > &wordSizeMap, string truncationList);
    void parseTruncationRecord(map<pair<mpz_class, int>, vector<int> > &wordSizeMap, string record);

    void generateAdderGraph(PAGSuite::adder_graph_t* adder_graph);
    void generateInputNode(PAGSuite::input_node_t* node);
    void generateOutputNode(PAGSuite::output_node_t* node);
    void generateRegisterNode(PAGSuite::register_node_t* node);
    void generateMuxNode(PAGSuite::mux_node_t* node);
    void generateConfAdderSubtractorNode(PAGSuite::conf_adder_subtractor_node_t* node);
    void generateAdderSubtractorNode(PAGSuite::adder_subtractor_node_t* node);

    string generateSignalName(std::vector<std::vector<int64_t> > factor, int stage=-1);
    string factorToString(std::vector<std::vector<int64_t> > factor);
    //    int computeWordSize(std::vector<std::vector<int64_t> > factor);

    vector<vector<int64_t> > output_coefficients;
    PAGSuite::adder_graph_t adder_graph;
    list<string> input_signals;

    list<output_signal_info> output_signals;

    int noOfInputs=-1;
    int noOfConfigurations=-1;

  };
}//namespace

#else
namespace flopoco {
    class IntConstMultShiftAddNew : public Operator {
    public:
        static void registerFactory();
    };
}
#endif // defined(HAVE_PAGLIB) && defined(HAVE_RPAGLIB) && defined(HAVE_SCALP)

#endif
