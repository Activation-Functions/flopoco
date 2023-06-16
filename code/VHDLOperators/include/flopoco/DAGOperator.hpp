/* Each Operator declared within the flopoco framework has 
   to inherit the class Operator and overload some functions listed below*/
#include "Operator.hpp"
namespace flopoco {

	// new operator class declaration
	class DAGOperator : public Operator {
	public:
		/* operatorInfo is a user defined parameter (not a part of Operator class) for
		   stocking information about the operator. The user is able to defined any number of parameter in this class, as soon as it does not affect Operator parameters undeliberatly*/
		string infile;

	public:
		// definition of some function for the operator    

		// constructor, defined there with two parameters (default value 0 for each)
		DAGOperator(OperatorPtr parentOp, Target* target, string infile);

		// destructor
		~DAGOperator() {};


		// Below all the functions needed to test the operator
		/* the emulate function is used to simulate in software the operator
		   in order to compare this result with those outputted by the vhdl operator */
		void emulate(TestCase * tc);


		/* function used to bias the (uniform by default) random test generator
		   See FPExp.cpp for an example */
		// TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui);

		/** Factory register method */ 
		static void registerFactory();


#ifdef DAGOPERATOR_IMPLEM
	private:
		void parse(string infile);
		void checkDAG(); /**< various static checks */
		void typeInference();
		void build();
		string entityName;

		/** global parameters */
		map<string, string> parameters; // parameter name, value 

		// The following few maps capture the AST built by the parser
		/** Signals */
		map<string,string> dagSignalList; // the second string is "Input", "Output", "Wire"
		map<string,int> dagSignalBitwidth; // 

		/* flopoco commands, built at parse time, invoked later */
		/** which flopoco Operator for each declared component */
		map<string, string> componentOperator;

		/** which Operator parameters for each declared component */
		map<string, vector<string>> componentParameters; // instanceName, reconstructed parameter list 
		/** which Component for each instance in the DAG */
		map<string, string> instanceComponent; // instanceName, componentName

		// The AST node is all string-based.
		// A node is either a signal or the name of an instance with its arguments 
		map<string, vector<string>> dagNode; /**< ultra simple AST */
		map<string, string> assignment; /**< ultra simple AST */
#endif
	};


}//namespace
