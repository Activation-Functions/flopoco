/* This is a dummy operator that should be used to build a new Target.
	 Syntesize it, then look at the critical path and transfer the obtained information in YourTarget.hpp and YourTarget.cpp  
*/
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"

/* This file contains a lot of useful functions to manipulate vhdl */
#include "flopoco/UserInterface.hpp"
#include "flopoco/utils.hpp"


namespace flopoco {

	class TargetModel : public Operator {
	public:


	public:
		TargetModel(Target* target, int type);

		~TargetModel() {};


		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui);
		
	private:
		int type; /**< The type of feature we want to model. See the operator docstring for options */

	};


}//namespace
