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

		AutoTest(std::string opName, const int testLevel);

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , std::vector<std::string> &args, UserInterface& ui);

		// TODO as of now this only returns TestBench n=1000. Find something better
		// TODO at some point we might want to get rid of strings
		static string defaultTestBenchSize(std::map<std::string, std::string> const & unitTestParam, int testLevel);

  private:
    int testLevel;
	};
};
#endif
