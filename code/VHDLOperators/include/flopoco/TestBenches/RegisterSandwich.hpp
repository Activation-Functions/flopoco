#ifndef REGISTERSANDWICH_HPP
#define REGISTERSANDWICH_HPP

/**
 * A wrapper is a VHDL entity that places registers before and after
 * an operator, so that you can synthesize it and get delay and area,
 * without the synthesis tools optimizing out your design because it
 * is connected to nothing.
 **/

#include "flopoco/InterfacedOperator.hpp"
namespace flopoco{

	class RegisterSandwich : public Operator
	{
	public:
		/**
		 * The RegisterSandwich constructor
		 * @param[in] target the target device
		 * @param[in] op the operator to be wrapped
		 **/
		RegisterSandwich(Target* target, Operator* op);

		/** The destructor */
		~RegisterSandwich();

		// User-interface stuff

		/**
		 * Factory method that parses arguments and calls the constructor
		 */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

	private:
		Operator* op_; /**< The operator to wrap */
	};
}
#endif
