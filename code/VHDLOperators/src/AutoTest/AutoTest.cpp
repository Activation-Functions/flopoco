#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <istream>
#include <map>
#include <optional>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include <boost/process.hpp>

#include "flopoco/AutoTest/AutoTest.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/UserInterface.hpp"

namespace {
namespace fs =  std::filesystem;
/**
 * @brief The TempDirectoryManager class
 * Manages all temporary directories created for the purpose
 * of autotests. All directories have a randomly-generated
 * unique ID.
 */
struct TempDirectoryManager {
private:
    bool deleteOnDestruct;
public:
    std::optional<fs::path> tmpPath;
    /**
     * @brief Constructor
     * @param deleteAtEnd
     */
    TempDirectoryManager(bool deleteAtEnd = false)
        : deleteOnDestruct { deleteAtEnd }
    {
      // Temporary name generation inspired from:
      // https://stackoverflow.com/a/58454949
        auto tmpDir = fs::temp_directory_path();
        std::uint64_t disambiguationTag = 0;
        std::random_device seedGen;
        std::mt19937 rng(seedGen());
        std::uniform_int_distribution<uint64_t> rand;
        fs::path path;
        size_t nbAttempts = 0;
        bool dirNameOk = false;

        while (!dirNameOk && nbAttempts < 1024) {
            std::stringstream sname;
            sname << "flopoco_tmp_" << std::hex << rand(rng);
            path = tmpDir / sname.str();
            dirNameOk = create_directory(path);
            nbAttempts += 1;
        }
        if (dirNameOk)
            tmpPath = path;
    }
    ~TempDirectoryManager() {
        if (deleteOnDestruct && tmpPath.has_value()) {
            fs::remove_all(tmpPath.value());
        }
    }
    /**
     * @brief inGoodState
     * @return true if tmp directory was successfully created.
     */
    bool inGoodState() {
        return tmpPath.has_value();
    }
};

/**
 * @brief The OperatorTester class
 */
class OperatorTester {
    static constexpr uint8_t HDLGenerationOK = 0b1;
    static constexpr uint8_t SimulationOK = 0b10;
    /**
     * @brief The TestConfig struct
     */
    struct TestConfig {
        std::map<std::string, std::string> parameters;
        std::string nbTestBenchCases;
        uint8_t status;
        std::vector<std::string> flopocoOut;
        std::vector<std::string> nvcOut;
        /**
         * @brief dumpFlopocoOut
         * @param outName
         */
        void dumpFlopocoOut(std::string_view outName) {
            std::ofstream out {std::string{outName}, std::ios::out};
            for (auto const & line : flopocoOut) {
                 out << line << "\n";
            }
        }
        /**
         * @brief dumpnvcOut
         * @param outName
         */
        void dumpnvcOut(std::string_view outName) {
            std::ofstream out{std::string{outName}, std::ios::out};
            for (auto const & line : nvcOut) {
                 out << line << "\n";
            }
        }
    };
private:
    std::vector<TestConfig> tests;
    bool hasRun;
    OperatorFactory const *const opFact;
    fs::path testRoot;
    int testLevel;
    /**
     * @brief commandLineForTestCase
     * @param testcase
     * @return
     */
    std::string commandLineForTestCase(TestConfig const &testcase) const {
      std::string commandLine = opFact->name();
        for (auto const &[paramName, paramValue] : testcase.parameters) {
             commandLine += " " + paramName + "=" + paramValue;
        }
        commandLine += " TestBench n=" + testcase.nbTestBenchCases;
        return commandLine;
    }
public:
    /**
     * @brief OperatorTester
     * @param fact
     * @param autotestOutputRoot
     * @param testLevel
     */
    OperatorTester(OperatorFactory const *fact, fs::path const &autotestOutputRoot, int testLevel)
        : hasRun {false},
          opFact {fact},
       testLevel (testLevel)
    {
        testRoot = autotestOutputRoot / opFact->name();
    }
    /**
     * @brief registerTests
     */
    void registerTests() {
        auto unitTestsList = opFact->unitTestGenerator(testLevel);
        auto paramNames = opFact->param_names();
        // Get the default values for factory parameters
        std::map<std::string, std::string> unitTestDefaultParam;
        for (auto param : paramNames) {
             string defaultValue = opFact->getDefaultParamVal(param);
             unitTestDefaultParam.insert(make_pair(param, defaultValue));
        }
        for (auto &paramlist : unitTestsList) {
            std::string testBenchString = "";
            auto unitTestParam {unitTestDefaultParam};
            // For each parameter specified in the
            // unit test parameters, we either update the
            // default value, or insert a new parameter
            // (TODO: see why we would have this one)
            // Special handling is given to TestBench to
            // ensure it arrives at the end of the command
            // line
            for (auto &[paramName, paramValue] : paramlist) {
                 if (unitTestParam.count(paramName)) {
                     unitTestParam[paramName] = paramValue;
                 } else if (paramName == "TestBench n=") {
                     testBenchString = paramValue;
                 } else {
                     unitTestParam.insert({paramName, paramValue});
                 }
            }
            if (testBenchString == "") {
                testBenchString = AutoTest::defaultTestBenchSize(unitTestParam, testLevel);
            }
            tests.emplace_back(TestConfig {
                    std::move(unitTestParam), std::move(testBenchString), 0
                }
            );
        }
        if (unitTestsList.empty())
            std::cout << "No unitTest method defined" << std::endl;
    }
/*
    void registerUnitTests() {
    	registerTests(-1);
    }
    void registerRandomTests() {
    	registerTests(0);
    }
*/
    /**
     * @brief getNbTests
     * @return
     */
    size_t getNbTests() {
    	return tests.size();
    }
    /**
     * @brief nbGenerationOK
     * @return
     */
    size_t nbGenerationOk () const {
    	assert(hasRun);
    	size_t res{0};
    	for (auto const & testCase : tests) {
             if (testCase.status & HDLGenerationOK)
                 res += 1;
    	}
    	return res;
    }
    /**
     * @brief nbSimulationOk
     * @return
     */
    size_t nbSimulationOk () const {
        assert(hasRun);
        size_t res{0};
        for (auto const & testCase : tests) {
             if (testCase.status & SimulationOK)
                 res += 1;
        }
        return res;
    }
    /**
     * @brief runTests
     */
    void runTests() {
        std::cout << endl << "Running tests for " << opFact->name() << ":\n";
        namespace bp = boost::process;
        if (!fs::create_directory(testRoot)) {
             fs::remove_all(testRoot);
             fs::create_directory(testRoot);
        }
        auto name = fs::absolute(UserInterface::getUserInterface().getExecName()).string();
        if (!fs::exists(name)) {
            // This is required when installing flopoco system-wide (e.g. /usr/local/bin):
            name = UserInterface::getUserInterface().getExecName();
        }
        auto detailed = testRoot / "detailed.txt";
        std::ofstream detailedFile{detailed, std::ios::out};
        auto bufferOut = testRoot / "buffer.txt";
        int id = 0;
        detailedFile << "TestID, Command line, VHDL Generation status, Simulation status\n";
        auto getLines = [&bufferOut](std::vector<std::string> & out) {
            std::ifstream in{bufferOut.string()};
            std::string line;
            while (std::getline(in, line)) {
            	out.emplace_back(line);
            }
        };
        auto nvcPath = bp::search_path("nvc");
        auto nbTests = tests.size();
        for (auto & testCase : tests) {
             auto command = commandLineForTestCase(testCase);
        #if 0  // nice but I had rather see the command line
             std::cout << "\33[2K\rRunning test " << (id + 1) << " / " << nbTests;
             std::cout.flush();
             fs::remove(bufferOut);
        #else
            std::cout << "Running test " << (id + 1) << " / " << nbTests << "    "   << " " << command << endl;
        #endif
            detailedFile << id << ", " << name << " " << command << ", ";
            auto flopocoStatus = bp::system(
                name + " " + command,
                (bp::std_out & bp::std_err) > bufferOut.string(),
                bp::start_dir(testRoot.string())
            );
            std::string nvcLine;
            getLines(testCase.flopocoOut);
            for (auto const & line : testCase.flopocoOut) {
                if (line.rfind("nvc", 0) == 0) {
                    nvcLine = line;
                }
            }
            // We managed to launch flopoco and it did execute properly
            if (flopocoStatus == 0 && nvcLine != ""
             && fs::exists(testRoot / "flopoco.vhdl"))
            {
                nvcLine.replace(0, 3, nvcPath.c_str());
                detailedFile << "1, ";
                testCase.status |= HDLGenerationOK;
                // Try to run nvc
                fs::remove(bufferOut);
                int nvcStatus;
                try {
                    nvcStatus = bp::system(
                        nvcLine,
                        (bp::std_err & bp::std_out) > bufferOut.string(),
                         bp::start_dir(testRoot.string()),
                         bp::throw_on_error
                    );
                } catch (bp::process_error &pe) {
                    std::cerr << "There is an issue with your nvc installation. "
                                 "Please fix it before running AutoTest." << endl
                              << "nvc command was: " << nvcLine << endl
                              << "Error: " << pe.what() << endl
                              << "FloPoCo will now graciously crash" << endl
                              << endl
                    ;
                    exit(EXIT_FAILURE);
                }
                getLines(testCase.nvcOut);
                if (nvcStatus == 0) {
                    testCase.status |= SimulationOK;
                    detailedFile << "1\n";
                } else {
                    detailedFile << "0\n";
                    std::stringstream errname;
                    errname << "nvc_err_" << id;
                    auto dest = testRoot / errname.str();
                    testCase.dumpnvcOut(dest.string());
                    std::cout << endl << "Failed at simulation step. "
                              << endl << "Log can be found in "
                              << dest.string()
                              << endl;
                    std::cout << "Command was:" << endl
                              << name << " "
                              << commandLineForTestCase(testCase) /*nvcLine*/
                              << endl;
                }
            } else {
                  detailedFile << "0, 0\n";
                  std::stringstream errname;
                  errname << "flopoco_err_" << id;
                  auto dest = testRoot / errname.str();
                  testCase.dumpFlopocoOut(dest.string());
                  std::cout << endl << "Failed at generation step. "
                            << endl << "Log can be found in "
                            << dest.string()
                            << endl;
                  std::cout << "Command was:" << endl
                            << name << " "
                            << commandLineForTestCase(testCase)
                            << endl;
            }
            ++id;
        }
        hasRun = true;
    }
    /**
     * @brief printStats
     * @param out
     */
    void printStats(std::ostream& out) {
        size_t nbTests = tests.size();
        if (nbTests == 0) {
            out << "No tests defined for "
                << opFact->name()
                << " at test level " << testLevel
                << endl;
        } else {
            out  << "Results for " << opFact->name() << endl;
            size_t nbGenOk {nbGenerationOk()}, nbSimOk {nbSimulationOk()};
            auto plural = [](int val) {
                if (val > 1)  {
                    return std::string_view("tests");
                } else {
                    return std::string_view("test");
                }
            };
            double percentageGen = 0.;
            out << "\t" << nbTests << " " << plural(nbTests) << " in total\n";
            if (nbTests > 0) {
                out << "\t" << nbGenOk << " correctly generated ("
                    << ((100.0 * nbGenOk) / nbTests)
                    << " \% of all tests)\n"
                ;
                out << "\t" << nbSimOk << " correctly simulated";
                if (nbGenOk > 0) {
                    out << " (" << ((100.0 * nbSimOk) / nbGenOk)
                        << " \% of correctly generated cases)";
                }
                out << "\n";
            }
        }
    }
};
} // namespace

// TODO: testDependences is fragile
namespace flopoco
{
  /**
 * @brief AutoTest::parseArguments
 * @param parentOp
 * @param target
 * @param args
 * @param ui
 * @return
 */
OperatorPtr AutoTest::parseArguments(
       OperatorPtr parentOp,
           Target* target,
   vector<string>& args,
    UserInterface& ui
) {
    string opName, output;
    ui.parseString(args, "Operator", &opName);
    int testLevel;
    ui.parseInt(args, "testLevel", &testLevel);
    ui.parseString(args, "output", &output);
    AutoTest AutoTest(opName, testLevel, output);
    return nullptr;
}

/**
 * @brief op_descriptor
 */
template<>
const OperatorDescription<AutoTest> op_descriptor<AutoTest> {
  "AutoTest",                 // Name
  "A tester for operators.",  // Description
  "AutoTest",                 // Category
  "",                         // See Also
  "Operator(string): name of the operator to test, All if we need to test all the operators;"
  "testLevel(int)=0: test level (0-3), "
      "0=only quick tests (< 1 second per operator), "
      "1=substantial tests, "
      "2=exhaustive tests, "
      "3=infinite tests (which produce random parameter combinations which may take forever);"
  "output(string)=random: explicit path for .csv generation output (otherwise random)",
    ""                        // Extra HTML Doc
};

/**
 * @brief AutoTest::AutoTest
 * @param opName
 * @param testLevel
 */
AutoTest::AutoTest(string opName, const int testLevel, string output) : testLevel(testLevel) {
    fs::path path(output);
    if (output == "random") {
        TempDirectoryManager tmpDirHolder;
        if (!(tmpDirHolder.inGoodState())) {
            throw "Creation of temporary directory is impossible";
        }
        path = tmpDirHolder.tmpPath->string();
    }
    fs::create_directories(path);
    std:cout << "All reporting will be done in "
             << path << std::endl
    ;
    FactoryRegistry& factRegistry = FactoryRegistry::getFactoryRegistry();
    std::map<std::string, OperatorTester> testerMap;
    set<string> testedOperator;
    bool doUnitTest = false;
    bool doRandomTest = false;
    bool allOpTest = false;
    bool testsDone = false;

    if (opName == "All" || opName == "all") {
    	doUnitTest = true;
    	doRandomTest = true;
    	allOpTest = true;
    } else if (opName == "AllUnitTest") {
    	doUnitTest = true;
    	allOpTest = true;
    } else if (opName == "AllRandomTest") {
    	doRandomTest = true;
    	allOpTest = true;
    } else {
    	doUnitTest = true;
    	doRandomTest = true;
    }
    // Select the Operator(s) to test
    if (allOpTest) {
    	for (auto f : factRegistry.getPublicRegistry())	{
             if (f->name() != "AutoTest" && !f->isHidden())
                 testedOperator.insert(f->name());
    	}
    } else {
    	auto f = factRegistry.getFactoryByName(opName);
    	if (!f->isHidden()) {
            testedOperator.insert(f->name());
    	}
        // Do we check for dependences ? No point really
    }
    // For each tested Operator, we run a number of tests defined in the Operator's unitTest method
    for (auto op: testedOperator) {
    	auto iter = testerMap.emplace(
            op, OperatorTester {
                factRegistry.getFactoryByName(op),
                path, testLevel
            }
        ).first;
    	auto& tester = iter->second;
    	// Register the tests
    	tester.registerTests();
/*
    	// First we register the unitTest for each tested Operator
    	if (doUnitTest) tester.registerUnitTests();
    	// Then we register random Tests for each tested Operator
    	if(doRandomTest) tester.registerRandomTests();
*/
    	// Real run of the tests
    	tester.runTests();
    	tester.printStats(cout);
    }
    // Build summary.csv and a few global stats
    fs::path summaryFilePath = path / "summary.csv";
    ofstream outputSummary(summaryFilePath);
    size_t totalTests = 0, genOK = 0, simOK = 0;
    outputSummary << "Operator, "
                  << "Tests, "
                  << "Generation, "
                  << "Simulation, "
                  << "%, "
                  << "Status, "       // These two columns are just placeholders
                  << "Passed Last\n"  // and are post-processed during 'deploy' stage on Gitlab.
    ;
    for (auto& [opName, tester] : testerMap) {
    	totalTests += tester.getNbTests();
    	genOK += tester.nbGenerationOk();
    	simOK += tester.nbSimulationOk();
        float percentage = 0;
        if (tester.getNbTests() > 0) {
            percentage= (float) tester.nbSimulationOk()
                      / (float) tester.getNbTests()
                      * 100.f;
        }
    	outputSummary << opName << ", "
                      << tester.getNbTests() << ", "
                      << tester.nbGenerationOk() << ", "
                      << tester.nbSimulationOk() << ", "
                      << percentage << ", "
                      << "Undefined, "     // 'status'
                      << 0 << std::endl;  // commit hash
    }
    outputSummary << "Total, "
                  << totalTests << ", "
                  << genOK << ", "
                  << simOK << ", "
                  << (float) totalTests / (float) (simOK) * 100.f << ", "
                  << "Undefined, "
                  << 0 << std::endl;
    cout << "Tests are finished, see summary in " << summaryFilePath.string() << endl;
    cout << "Total number of tests  " << totalTests << endl;
    cout << "Code generation OK     " << genOK << endl;
    cout << "Simulation OK          " << simOK << endl;
}

/**
 * @brief AutoTest::defaultTestBenchSize
 * @param unitTestParam
 * @param testLevel
 * @return
 */
string AutoTest::defaultTestBenchSize(
  map<string,string> const& unitTestParam,
                        int testLevel
){
    // This was definitely fragile, we can't rely on information extracted this way
    // Better make it explicit in the unitTest methods
    string testBench;
#if 0
    if (testLevel == TestLevel::QUICK) {
        testBench = "100";
    } else {
        testBench = "1000";
    }
#else
    testBench = "-1";
#endif
    return testBench;
}
} // namespace flopoco
