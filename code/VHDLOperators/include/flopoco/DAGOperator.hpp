/* Each Operator declared within the flopoco framework has 
   to inherit the class Operator and overload some functions listed below*/
#include "Operator.hpp"
namespace flopoco {

	// new operator class declaration
	class DAGOperator : public Operator {
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
		string infile;
		void parse();
		void typeInference();
		void build();
		void check(); /**< final static checks */

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
		map<string, vector<string>> componentParameters; // componentName, reconstructed parameter list 
		/** which Component for each instance in the DAG */
		map<string, string> instanceComponent; // uniqueInstanceName, componentName
		map<string, pair <size_t, size_t>> instanceLineInfo; // uniqueInstanceName, (line, column)

		// The AST node is all string-based.
		map<string, vector<string>> dagNode; /**< ultra simple AST. 
			  maps an instance name to its arguments (either signal names or instance names) */
		map<string, string> assignment; /**< ultra simple AST. first is LHS signal name, second is  */
#endif
	};


}//namespace
