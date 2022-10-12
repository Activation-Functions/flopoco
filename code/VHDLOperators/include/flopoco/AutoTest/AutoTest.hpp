#ifndef AutoTest_hpp
#define AutoTest_hpp

#include <stdio.h>
#include <string>
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco{

	class AutoTest 
	{

	public:

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , std::vector<std::string> &args, UserInterface& ui);

		AutoTest(std::string opName, bool testDependences = false);

	private:

		string defaultTestBenchSize(std::map<std::string, std::string> * unitTestParam);

	};
};
#endif
