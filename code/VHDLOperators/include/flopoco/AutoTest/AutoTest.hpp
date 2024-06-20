#ifndef AutoTest_hpp
#define AutoTest_hpp

#include <stdio.h>
#include <string>
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco
{
/**
 * @brief The TestLevel enum
 */
enum TestLevel {
        QUICK = 0, // < 1 second per operator
  SUBSTANTIAL = 1,
   EXHAUSTIVE = 2,
     INFINITE = 3  // May take forever...
};
/**
 * @brief The AutoTest class
 */
class AutoTest {
public:
  /**
     * @brief AutoTest
     * @param opName
     * @param testLevel
     */
    AutoTest(std::string opName, const int testLevel, string output);
    /**
     * @brief parseArguments
     * @param parentOp
     * @param target
     * @param args
     * @param ui
     * @return
     */
    static OperatorPtr parseArguments(
                    OperatorPtr parentOp,
                        Target* target ,
      std::vector<std::string>& args,
                 UserInterface& ui
    );
    // TODO: as of now this only returns TestBench n=1000. Find something better
    // TODO: at some point we might want to get rid of strings
    /**
     * @brief defaultTestBenchSize
     * @param unitTestParam
     * @param testLevel
     * @return
     */
    static string defaultTestBenchSize(
        std::map<std::string, std::string> const& unitTestParam,
        int testLevel
    );
private:
    // See TestLevel enum for possible values
    int testLevel;
};

}
#endif
