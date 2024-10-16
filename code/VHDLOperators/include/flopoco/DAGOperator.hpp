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
		/** A dictionnary of DAG signal names (including implicit ones).
				Also maps each DAG signal name to its type,	which may be  "Input", "Output", or "Wire".	*/
		map<string,string> dagSignalList;

		/** maps each DAG signal name (including implicit ones) to its inferred bit width */
		map<string,int> dagSignalBitwidth; 

		/** maps each declared component to its flopoco Operator name */
		map<string, string> componentOperator;

		/** maps a componentName to its reconstructed parameter list*/
		map<string, vector<string>> componentParameters;

		/** maps the uniqueInstanceName of a DAG node to its componentName */
		map<string, string> instanceComponent; 

		/** maps the uniqueInstanceName of a DAG node to its (line, column) in the source file for each DAG instance */
		map<string, pair <size_t, size_t>> instanceLineInfo;

		/** maps the uniqueInstanceName of a DAG node to its FloPoCo Operator */
		map<string, OperatorPtr> instanceOperator; 

		/** maps the uniqueInstanceName of a DAG node to the input data of its FloPoCo Operator: pairs of (name, bitwidth) */
		map<string, vector<pair<string,int>>> instanceInputs; 

		/** maps the uniqueInstanceName of a DAG node to the output data of its FloPoCo Operator: pairs of (name, bitwidth) */
		map<string, vector<pair<string,int>>> instanceOutputs; 

		// The AST node is all string-based.
		map<string, vector<string>> dagNode; /**< ultra simple AST. 
			  maps an instance name to its arguments (either signal names or instance names) */
		map<string, string> assignment; /**< ultra simple AST. first is LHS signal name, second is  */
#endif
	};


}//namespace
