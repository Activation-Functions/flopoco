#ifndef GENERICADDSUB_H
#define GENERICADDSUB_H

#include "flopoco/Operator.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco
{
/// \brief The IntAddSub class is a reconfigurable add/subtractor with flexible input signs, including a ternary adder
/// It provides a basic VHDL model but uses tatget optimized versions whenever possible
  class IntAddSub : public Operator
  {
  public:

    /// \brief The ADD_SUB_FLAGS enum. Properties for primitive adders.
    /// !!!! remove this once IntConstMultShiftTypes is replaced !!!
    enum ADD_SUB_FLAGS
    {
      TERNARY = 0x1, //!< Adder has three inputs
      NEG_X = 0x2, //!< Subtract X input
      NEG_Y = 0x4, //!< Subtract Y input
      NEG_Z = 0x8, //!< Subtract Z input, if ternary
      CONF_X = 0x10, //!< Make X input configurable
      CONF_Y = 0x20, //!< Make Y input configurable
      CONF_Z = 0x40, //!< Make Z input configurable, if ternary
      CONF_ALL = 0x70, //!< Make all inputs configurable, if possible
      SIGN_EXTEND = 0x80 //!< Make sign extension, currently not supported, could be removed
    };

  private:
    const bool isTernary;
    const bool xNegative;
    const bool yNegative;
    const bool zNegative;
    const bool xConfigurable;
    const bool yConfigurable;
    const bool zConfigurable;
    const bool isSigned;

    int wIn; //word size of the input
    int wOut; //word size of the output
  public:

    /// \brief IntAddSub constructor
    /// \param target The current Target
    /// \param wIn The input word size of the generated adder
    ///
    IntAddSub(Operator *parentOp, Target *target, const uint32_t &wIn, const bool isSigned=false, const bool isTernary=false, const bool xNegative=false, const bool yNegative=false, const bool zNegative=false, const bool xConfigurable=false, const bool yConfigurable=false, const bool zConfigurable=false);

    /// \brief getInputName returns the name of the input signal or the conf signal of the specific index
    /// \param index which input to return
    /// \param c_input wether to return the input signal or the configuration signal
    /// \return Name of the input
    ///
    std::string getInputName(const uint32_t &index, const bool &c_input = false) const;

    /// \brief getOutputName return the name of the output signal
    /// \return Name of the output signal
    ///
    std::string getOutputName() const;

    /// \brief getInputCount returns the input count of the adder
    /// \return Input count of the adder (3 for ternary otherwise 2)
    ///
    const uint32_t getInputCount() const;

    /// \brief printFlags returns the properties as a string
    /// \return Property string.
    ///
    std::string printFlags() const;

    ~IntAddSub()
    {}

    void emulate(TestCase *tc);
    void buildStandardTestCases(TestCaseList *tcl);
    static TestList unitTest(int testLevel);

  public:

    /// \brief buildXilinxIntAddSub builds the Xilinx implementation of the primitive adder
    /// \param target The current Target
    /// \param wIn The input wordsize
    ///
    void buildXilinxIntAddSub(Target *target, const uint32_t &wIn);


    ///
    /// \brief buildCommon builds the VHDL-code implementation of the primitive adder
    /// \param target The current Target
    /// \param wIn The input wordsize
    ///
    void buildCommon(Target *target, const uint32_t &wIn);

    // User-interface stuff
    /** Factory method */
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui);
  private:
    void generateInternalInputSignal(string name, int wIn, bool isConfigurable, bool isNegative);
  };
}//namespace

#endif
