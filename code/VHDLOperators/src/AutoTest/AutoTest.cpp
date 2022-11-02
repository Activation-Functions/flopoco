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
	struct TempDirectoryManager {
		std::optional<fs::path> tmpPath;
		TempDirectoryManager(bool deleteAtEnd=false):deleteOnDestruct{deleteAtEnd}{
			// Temporary name generation inspired from
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

		bool inGoodState() {
			return tmpPath.has_value();
		}

		~TempDirectoryManager() {
			if (deleteOnDestruct && tmpPath.has_value()){
				fs::remove_all(tmpPath.value());
			}
		}
		private:
			bool deleteOnDestruct;
	};

	class OperatorTester
	{
		static constexpr uint8_t HDLGenerationOK = 0b1;
		static constexpr uint8_t SimulationOK = 0b10;
		struct TestConfig {
			std::map<std::string, std::string> parameters;
			std::string nbTestBenchCases;
			uint8_t status;
			std::vector<std::string> flopocoOut;
			std::vector<std::string> nvcOut;

			void dumpFlopocoOut(std::string_view outName) {
				std::ofstream out{std::string{outName}, std::ios::out};
				for (auto const & line : flopocoOut) {
					out << line << "\n";
				}
			}
			
			void dumpnvcOut(std::string_view outName) {
				std::ofstream out{std::string{outName}, std::ios::out};
				for (auto const & line : nvcOut) {
					out << line << "\n";
				}
			}
		};

		std::string commandLineForTestCase(TestConfig const &testcase) const
		{
			auto commandLine = opFact->name();
			for (auto const &[paramName, paramValue] :
			     testcase.parameters) {
				commandLine +=
				    " " + paramName + "=" + paramValue;
			}
			commandLine += " TestBench n=" + testcase.nbTestBenchCases;
			return commandLine;
		}

		std::vector<TestConfig> tests;
		bool hasRun;

		OperatorFactory const *const opFact;
		fs::path testRoot;

		void registerTests(int index)
		{
			auto unitTestsList = opFact->unitTestGenerator(index);
			auto paramNames = opFact->param_names();

			// Get the default values for factory parameters
			std::map<std::string, std::string> unitTestDefaultParam;
			for (auto param : paramNames) {
				string defaultValue =
				    opFact->getDefaultParamVal(param);
				unitTestDefaultParam.insert(
				    make_pair(param, defaultValue));
			}
			for (auto &paramlist : unitTestsList) {
				std::string testBenchString = "";
				auto unitTestParam{unitTestDefaultParam};
				// For each parameter specified in the
				// unit test parameters, we either update the
				// default value, or insert a new parameter
				// (TODO: see why we would have this one)
				// Special handling is given to TestBench to
				// ensure it arrives at the end of the command
				// line
				for (auto &[paramName, paramValue] :
				     paramlist) {
					if (unitTestParam.count(paramName)) {
						unitTestParam[paramName] =
						    paramValue;
					} else if (paramName ==
						   "TestBench n=") {
						testBenchString = paramValue;
					} else {
						unitTestParam.insert(
						    {paramName, paramValue});
					}
				}
				if (testBenchString == "") {
					testBenchString = AutoTest::defaultTestBenchSize(unitTestParam);
				}
				tests.emplace_back(TestConfig{std::move(unitTestParam),
						   std::move(testBenchString), 0});
			}
			if (unitTestsList.empty())
				std::cout << "No unitTest method defined"
					  << std::endl;
		}

	public:
		OperatorTester(OperatorFactory const *fact,
			       fs::path const &autotestOutputRoot)
		    : hasRun{false}, opFact{fact}
		{
			testRoot = autotestOutputRoot / opFact->name();
		}

		void registerUnitTests() {
			registerTests(-1);
		}

		void registerRandomTests() {
			registerTests(0);
		}

		size_t getNbTests() {
			return tests.size();
		}

		size_t nbGenerationOK () const {
			assert(hasRun);
			size_t res{0};
			for (auto const & testCase : tests) {
				if (testCase.status & HDLGenerationOK) res += 1;
			}
			return res;
		}

		size_t nbSimulationOk () const {
			assert(hasRun);
			size_t res{0};
			for (auto const & testCase : tests) {
				if (testCase.status & SimulationOK) res += 1;
			}
			return res;
		}

		void runTests() {
			std::cout << "Running tests for " << opFact->name() << ":\n";
			namespace bp = boost::process;
			if (!fs::create_directory(testRoot)) {
				fs::remove_all(testRoot);
				fs::create_directory(testRoot);
			}
			auto name = fs::absolute(UserInterface::getUserInterface().getExecName()).string();
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
				std::cout << "\33[2K\rRunning test " << (id + 1) << " / " << nbTests;
				std::cout.flush();
				fs::remove(bufferOut);
				auto command = commandLineForTestCase(testCase);
				detailedFile << id << ", " << name << " " << command << ", ";
				auto flopocoStatus =  bp::system(name + " " + command, (bp::std_out & bp::std_err) > bufferOut.string(), bp::start_dir(testRoot.string())); 
				std::string nvcLine{""};
				getLines(testCase.flopocoOut);
				for (auto const & line : testCase.flopocoOut) {
					if (line.rfind("nvc", 0) == 0) {
						nvcLine = line;
					}
				}
				// We managed to launch flopoco and it did execute properly
				if (flopocoStatus == 0 && nvcLine != "" && fs::exists(testRoot / "flopoco.vhdl")){
					nvcLine.replace(0, 3, nvcPath.c_str());
					detailedFile << "1, ";
					testCase.status |= HDLGenerationOK;
					// Try to run nvc
					fs::remove(bufferOut);
					int nvcStatus;
					try {
						nvcStatus = bp::system(
						    nvcLine,
						    (bp::std_err &
						     bp::std_out) >
								bufferOut.string(),
						    bp::start_dir(
								testRoot.string()),
						    bp::throw_on_error);
					} catch (bp::process_error &pe) {
						std::cerr << "There is an issue with your nvc installation. Please fix it before running AutoTest.\n"
								  << "nvc command was: " << nvcLine << "\n"
								  << "Error: " << pe.what() << "\n"
								  << "FloPoCo will now graciously crash.\n\n";
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
						std::cout << "\nFailed at simulation step.\nLog can be found in " << dest.string() << "\n";
						std::cout << "Command was:\n" << nvcLine << "\n";
					}
				} else {
					detailedFile << "0, 0\n";
					std::stringstream errname;
					errname << "flopoco_err_" << id;
					auto dest = testRoot / errname.str();
					testCase.dumpFlopocoOut(dest.string());
					std::cout << "\nFailed at generation step.\nLog can be found in " << dest.string() << "\n";
					std::cout << "Command was:\n" << name << " " << commandLineForTestCase(testCase) << "\n";
				}
				++id;
			}
			hasRun = true;
			std::cout << "\n";
		}

		void printStats(std::ostream& out) {
			out << "Tests for " << opFact->name() << ":\n";
			size_t nbGenOk{nbGenerationOK()}, nbSimOk{nbSimulationOk()};
			auto plural = [](int val){if (val > 1) return std::string_view{"tests"}; else return std::string_view{"test"};};
			size_t nbTests = tests.size();
			double percentageGen = 0.;
			out << "\t" << nbTests << " " << plural(nbTests) << " in total\n";
			if (nbTests > 0) {
				out << "\t" << nbGenOk << " correctly generated (" << ((100.0 * nbGenOk) / nbTests) << " \% of all tests)\n";
				out << "\t" << nbSimOk << " correctly simulated";
				if (nbGenOk > 0) {
					out << " (" << ((100.0 * nbSimOk) / nbGenOk) << " \% of correctly generated cases)";
				}
				out << "\n";
			}
		}
	};
} // namespace
// TODO: testDependences is fragile
namespace flopoco
{

	OperatorPtr AutoTest::parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui)
	{
		string opName;
		ui.parseString(args, "Operator", &opName);

		AutoTest AutoTest(opName);

		return nullptr;
	}

	template<>
	const OperatorDescription<AutoTest> op_descriptor<AutoTest> {
		"AutoTest", // name
		"A tester for operators.",
		"AutoTest",
		"", //seeAlso
		"Operator(string): name of the operator to test, All if we need to test all the operators;",
		""
	};

	AutoTest::AutoTest(string opName)
	{
		TempDirectoryManager tmpDirHolder{};
		if (!(tmpDirHolder.inGoodState())) {
			throw "Creation of temporary directory is impossible";
		}
		std:cout << "All reporting will be done in " << tmpDirHolder.tmpPath->string() << std::endl;
		FactoryRegistry& factRegistry = FactoryRegistry::getFactoryRegistry();

		std::map<std::string, OperatorTester> testerMap;
		
		set<string> testedOperator;

		bool doUnitTest = false;
		bool doRandomTest = false;
		bool allOpTest = false;
		bool testsDone = false;

		if(opName == "All" || opName == "all")
		{
			doUnitTest = true;
			doRandomTest = true;
			allOpTest = true;
		}
		else if(opName == "AllUnitTest")
		{
			doUnitTest = true;
			allOpTest = true;
		}
		else if(opName == "AllRandomTest")
		{
			doRandomTest = true;
			allOpTest = true;
		}
		else
		{
			doUnitTest = true;
			doRandomTest = true;
		}

		// Select the Operator(s) to test
		if(allOpTest)
		{
			for (auto facto : factRegistry.getPublicRegistry())	{
				if(facto->name() != "AutoTest")
					testedOperator.insert(facto->name());
			}
		} else {
			auto opFact = factRegistry.getFactoryByName(opName);
			testedOperator.insert(opFact->name());
				// Do we check for dependences ?
		}



		// For each tested Operator, we run a number of tests defined in the Operator's unitTest method
		for(auto op: testedOperator)	{
			auto iter = testerMap.emplace(op, OperatorTester{factRegistry.getFactoryByName(op), *(tmpDirHolder.tmpPath)}).first;
			auto& tester = iter->second;
			// First we register the unitTest for each tested Operator
			if (doUnitTest) tester.registerUnitTests();
			// Then we register random Tests for each tested Operator
			if(doRandomTest) tester.registerRandomTests();
			
			// Real run of the tests
			tester.runTests();
			tester.printStats(cout);
		}

		ofstream outputSummary{"summary.csv"};
		outputSummary << "Operator Name, Total Tests, Generation OK, Simulation Ok\n";
		for (auto& [opName, tester] : testerMap) {
			outputSummary << opName << ", " 
						  << tester.getNbTests() << ", " 
						  << tester.nbGenerationOK() << ", " 
						  << tester.nbSimulationOk() << "\n";
		}

		cout << "Tests are finished" << endl;
	}

	string AutoTest::defaultTestBenchSize(map<string,string> const & unitTestParam)
	{
		// This  was definitely fragile, we can't rely on information extracted this way
		// Better make it explicit in the unitTest methods

#if 0
		string testBench = " TestBench n=";

		int bitsSum = 0;

		map<string,string>::iterator itParam;
		for(itParam = unitTestParam->begin(); itParam != unitTestParam->end(); ++itParam)	{
			// We look for something that looks like an input 
			//cerr << itParam->first << endl;
			if( (itParam->first.substr(0,1) == "w")	|| (itParam->first.find("In") != string::npos)) {
				bitsSum += stoi(itParam->second);
			}
		}

		if(bitsSum >= 8) {
			testBench += "1000";
		}
		else	{
			testBench += "-2";
		}
#endif

		string testBench = "1000";
		
		return testBench;
	}
};
