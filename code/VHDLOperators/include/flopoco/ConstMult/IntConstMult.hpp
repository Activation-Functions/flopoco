#ifndef FLOPOCO_INTCONSTMULT_HPP
#define FLOPOCO_INTCONSTMULT_HPP

#include "flopoco/Operator.hpp"

/**
	@brief Integer constant multiplication.

	IntConstMult is the main interface to constant multipliers.
  It basically creates the instances of the highly optimized variants using shift and add or KCM

*/


namespace flopoco{

	class IntConstMult : public Operator
	{
	public:

		/** @brief The standard constructor, inputs the number to implement */
		IntConstMult(OperatorPtr parentOp, Target* target, const int wIn, const string constants, const string method, int wOut, bool isSigned=true);

//    void emulate(TestCase *tc);
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

    void emulate(TestCase *tc);

//    void buildStandardTestCases(TestCaseList *tcl);

    static TestList unitTest(int testLevel);


  private:
    enum constMultClassType {SCM, MCM, CMM} constMultClass;
    string factorToString(std::vector<mpz_class> &coeff, constMultClassType constMultClass);

    std::vector<std::string> explode(std::string & s, char delim);
    void implementSCM();

    std::vector<std::vector<mpz_class> > coeffs;  // inner vector = weights for each input, outer vector = different outputs

    Target* target;
    string constants;
    string method;
    int wIn;
    int wOut;
    int noOfInputs;
    int noOfOutputs;
    bool isSigned;

  };
}


#endif //FLOPOCO_INTCONSTMULT_HPP
