#ifndef INTMULTIADDER_HPP
#define INTMULTIADDER_HPP
#include "flopoco/BitHeap/BitHeap.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/UserInterface.hpp"



namespace flopoco{

	class IntMultiAdder : public Operator
	{
	public:

		/**
		 * @brief 
		 */
		IntMultiAdder(
								 OperatorPtr parentOp,
								 Target* target, 
								 unsigned int wIn,
								 unsigned int n,
								 bool signedIn,
								 unsigned int wOut=0
								 );
		

		void emulate(TestCase* tc);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args, UserInterface& ui);

		static TestList unitTest(int index);
		

	private:
		unsigned int wIn;
		unsigned int n;
		bool signedIn;
		unsigned int wOut;
	};

}


#endif
