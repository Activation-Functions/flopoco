/*
  A test bench generator for FloPoCo.

  Authors: Cristian Klein, Florent de Dinechin, Nicolas Brunie

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2017.
  All rights reserved.

 */

#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include "flopoco/Operator.hpp"
#include "flopoco/TestBenches/TestBench.hpp"
#include "flopoco/TestBenches/TestCase.hpp"
#include "flopoco/utils.hpp"

using namespace std;

namespace flopoco{


	TestBench::TestBench(Target* target, Operator* op_, int64_t n_, bool fromFile_):
		Operator(nullptr, target), op(op_), n(n_), fromFile(fromFile_)
	{
		//We do not set the parent operator to this operator
		setNoParseNoSchedule();

		useNumericStd();
		// This allows the op under test to know how long it is being tested.
		// useful only for testing the long acc, but who knows.
		op->numberOfTests = n;

		srcFileName="TestBench";
		setNameWithFreqAndUID("TestBench_" + op->getName());

		//		REPORT(LogLevel::MESSAGE,"Test bench for "+ op->getName());

		// initialization of flopoco random generator
		// TODO : has to be initialized before any use of getLargeRandom or getRandomIEEE...
		//        maybe best to be placed in main.cpp ?
		FloPoCoRandomState::init(n);

		if(n == -2) {
			REPORT(LogLevel::MESSAGE,"Generating the exhaustive test bench, this may take some time");
			n = op-> buildExhaustiveTestCaseList(&tcl);
			REPORT(LogLevel::MESSAGE, n << " test cases have been generated");
		}
		else
			{
				// Generate the standard and random test cases for this operator
				op-> buildStandardTestCases(&tcl);
				// initialization of randomstate generator with the seed base on the number of
				// random testcase to be generated
				op-> buildRandomTestCaseList(&tcl, n);
			}

		simulationTime = 10*(
												 1 // reset time
												 + op->getPipelineDepth()
												 +  tcl.getNumberOfTestCases() // number of simulation steps 
												 )+ 2  ; // we read 2ns after clock rising edge
 
		
		// flags used to know if we need to generate the fp_equal functions or not.
		hasFPOutputs=false;
		hasIEEEOutputs=false;

		// The instance
		//  portmap for the inputs and outputs
		for(int i=0; i < op->getIOList().size(); i++){
			Signal* s = op->getIOListSignal(i);
<<<<<<< HEAD
			// Instance does not declare the output signals anymore, need to do it here
			declare(s->getName(), s->width(), s->isBus());
			inPortMap (s->getName(), s->getName());
=======
		
			if(s->type() == Signal::in) {
				declare(s->getName(), s->width(), s->isBus());
				inPortMap (s->getName(), s->getName());
			}
			if(s->type() == Signal::out) {
				outPortMap (s->getName(), s->getName());
				if (s->isFP()) {
					hasFPOutputs=true;
				}
				if (s->isIEEE()) {
					hasIEEEOutputs=true;	
				}
				
			}
>>>>>>> 86194a86 (refactoring, currently cleaning up code but adding bugs)
		}


		
		// add clk and rst
		declare("clk");
		declare("rst");
		// declaration and assignation of the clock enable signal
		if (op_->hasClockEnable()) {
			for (int stage = op->getMinInputCycle(); stage < op->getMaxOutputCycle(); stage++ ) {
				vhdl << tab << declare("ce_" + to_string(stage+1)) << " <= '1';" << endl;
			}
		}

		// The VHDL for the instance
		//vhdl << endl << instance(op, "test", false) << endl;
		//subComponentList_.clear(); // it is unfortunately set by instance()

		// Doing the VHDL for the instance myself
		// Remove all the sanity checks
		
		string instanceName = "test";
		vhdl << tab << instanceName << ": " << op->getName() << endl;
	  // INSERTED PART FOR PRIMITIVES
		if( !op->getGenerics().empty() ) {
			vhdl << tab << tab << "generic map ( ";
			std::map<string, string>::iterator it = op->getGenerics().begin();
			vhdl << it->first << " => " << it->second;

			for( ++it; it != op->getGenerics().end(); ++it  ) {
				vhdl << "," << endl << tab << tab << it->first << " => " << it->second;
			}

			vhdl << ")" << endl;
		}
		// INSERTED PART END
		 vhdl << tab << tab << "port map ( ";

		// build vhdl and erase portMap_
		if(op->isSequential())	{
			vhdl << "clk  => clk";
			if (op->hasReset())
			  vhdl << "," << endl << tab << tab << "           rst  => rst";
			if(op->hasClockEnable()) {
				for (int stage = op->getMinInputCycle(); stage < op->getMaxOutputCycle(); stage++ ) {
					vhdl << "," << endl << tab << tab << "           ce_" << stage+1 << " => ce_" << stage+1;
				}
			}				
		}
		// code for inputs and outputs
		for(int i=0; i < op->getIOListSize(); i++){
			Signal* s = op->getIOListSignal(i);
			if (i != 0 || op->isSequential())
				vhdl << "," << endl <<  tab << tab << "           ";
			vhdl << s->getName() << " => " << s->getName();
		}
		vhdl << ");" << endl;

		setSequential();

		if (fromFile)
			generateTestFromFile();
		else
			generateTestInVhdl();
	}



	void TestBench::generateLineTestFunction(ostream & o) {
		vector<Signal*> outputSignalVector = op->getOutputList();
		o << tab << "function testLine(testCounter:integer; expectedOutputS: string(1 to 10000)";
		for(Signal* s: outputSignalVector){
			o << "; "<< s->getName() << ": std_logic_vector (" << s->width()-1 << " downto 0)";
		}
		o << ") return boolean is" << endl;
		o << tab << tab << "variable expectedOutput: line;" << endl;
		o << tab << tab << "variable possibilityNumber : integer;" << endl;
		o << tab << tab << "variable testSuccess: boolean;" << endl;
		o << tab << tab << "variable errorMessage: string(1 to 10000);" << endl;
		//		o << tab << tab << "variable tmpErrorMessage: line;" << endl;
		//    o << tab << tab << "variable errorMessage: line;" << endl;
		for(Signal* s: outputSignalVector){
			o << tab << tab << "variable testSuccess_" << s->getName() << ": boolean;" << endl;
			o << tab << tab << "variable expected_" << s->getName() << ": bit_vector (" << s->width()-1 << " downto 0); -- for list of values" << endl;
			o << tab << tab << "variable inf_" << s->getName() << ": bit_vector (" << s->width() -1 << " downto 0); -- for intervals" << endl;
			o << tab << tab << "variable sup_" << s->getName() << ": bit_vector (" << s->width() -1 << " downto 0); -- for intervals" << endl;
		}
		
		o << tab << "begin" << endl;
		o << tab << tab << "write(expectedOutput, expectedOutputS);" << endl;
		for(Signal* s: outputSignalVector){
			// o << tab << tab << "errorMessage := \"\";" << endl;
			o << tab << tab << "read(expectedOutput, possibilityNumber); -- for " << s->getName() << endl;
			o << tab << tab << "if possibilityNumber = 0 then" << endl;
			o << tab << tab << tab << "-- TODO define what it means to have 0 possible output. Currently it means a test fails..." << endl;
			o << tab << tab << "end if;" << endl;
			o << tab << tab << "if possibilityNumber > 0 then -- a list of values" << endl;
			o << tab << tab << "testSuccess_" << s->getName() << " := false;" << endl;
			o << tab <<tab << tab << "for i in 1 to possibilityNumber loop" << endl;
			o << tab << tab << tab << tab << "read(expectedOutput, expected_" << s->getName() << ");" << endl;
			//			o << tab << tab << tab << tab << "errorMessage := errorMessage & \" \" & expected_" << s->getName()<< ";" << endl;
			o << tab << tab << tab << tab << "if " << s->getName() << " = to_stdlogicvector(expected_" << s->getName() << ") then" << endl;
			o << tab << tab << tab << tab << tab << "testSuccess_" << s->getName() << " := true;"  << endl;
			o << tab << tab << tab << tab << "end if;" << endl;
			o << tab << tab << tab << tab << "end loop;" << endl;
			o << tab << tab << "end if;" << endl;
			o << tab << tab << "if possibilityNumber < 0  then -- an interval" << endl;
			o << tab << tab << tab << "read(expectedOutput, inf_" << s->getName() << ");" << endl;
			o << tab << tab << tab << "read(expectedOutput, sup_" << s->getName() << ");" << endl;
			o << tab << tab << tab << "if possibilityNumber =-1  then -- an unsigned interval" << endl;
			o << tab << tab << tab << tab  << "testSuccess_" << s->getName() << " := (" << s->getName() << " >= to_stdlogicvector(inf_" << s->getName() << ")) or (" << s->getName() << " <= to_stdlogicvector(sup_" << s->getName() << "));" << endl;
			o << tab << tab << tab << "end if;" << endl;
			o << tab << tab << "end if;" << endl;
			o << tab << tab << "if testSuccess_" << s->getName() << " = false then" << endl;
			o << tab << tab << tab << "report(\"Test number \" & integer'image(testCounter) & \", incorrect output for " << s->getName() << ": \" & lf & \" expected values: \" ";
			// Here I should be able to output the list of values but VHDL sucks
			o << " & lf & \"          result: \" & str(" << s->getName() << ") ) severity error;" << endl;
			o << tab << tab << "end if;" << endl;
			o << tab << tab << "" << endl;
		}
		o << tab << tab << "testSuccess := true";
		for(Signal* s: outputSignalVector){
			o << " and testSuccess_" << s->getName();
		}
		o << ";" << endl;
		o << tab << tab << "return testSuccess;" << endl;
		o << tab << "end testLine;" << endl;
		
	}

#if 0 // TODO remove me
	// produce one VHDL statement that goes in a process and sets the "testSuccess_varName" variable
	// This VHDL inputs a testNumber and an array of possibleOutputs
	string TestBench::oneTestVHDL(Signal* s, string prepend) {
		ostringstream o;
		string varName=s->getName();
		tab << "function testOneOutput(possibilityNumber:integer; b : std_logic_vector) return boolean is\n" <<
			tab << "begin\n" <<

		o << prepend << "testSuccess_" << varName << " := false;" << endl;
		o << prepend << "if possibilityNumber=-1 then -- interval "  << endl;
		o << prepend <<  tab << "-- TODO "  << endl;
		o << prepend << "end if;"  << endl;
		o << prepend << "if  possibilityNumber>0 then -- list of possible values "  << endl;
		o << prepend << tab << "for i in 1 to possibilityNumber loop  "  << endl;
		o << prepend << tab << tab << "if "<<varName<<" = to_stdlogicvector(expected_"<<varName << "(i)) then testSuccess_" << varName << " := true ; end if;"  << endl;
		o << prepend << tab << "end loop;" << endl;
		o << prepend << "end if;" << endl;

		return o.str();
	}

#endif

	/* Generating the tests using a file to store the IO, allow to have a lot of IOs without
	 * increasing the VHDL compilation time
	 */
	void TestBench::generateTestFromFile() {
		vector<Signal*> inputSignalVector = op->getInputList();
		vector<Signal*> outputSignalVector = op->getOutputList();
			
		// In order to generate the file containing inputs and expected output in a correct order
		// we will store the use order for file decompression
		list<string> IOorderInput;
		list<string> IOorderOutput;


		vhdl << tab << "-- Process that sets the inputs  (read from a file) " << endl;
		vhdl << tab << "process" <<endl;
		vhdl << tab << tab << "variable input, expectedOutput : line; " << endl;  // variables to read a line
		vhdl << tab << tab << "variable tmpChar : character;" << endl; // variable to store a character (escape between inputs)
		vhdl << tab << tab << "file inputsFile : text is \"test.input\"; " << endl; // declaration of the input file

		/* Variable to store value for inputs and expected outputs*/
		for(int i=0; i < op->getIOList().size(); i++){
			Signal* s = op->getIOListSignal(i);
			vhdl << tab << tab << "variable V_" << s->getName();
			/*if (s->width() != 1)*/ vhdl << " : bit_vector("<< s->width() - 1 << " downto 0);" << endl;
			//else vhdl << " : bit;" << endl;
		}

		/* The process that resets then sets inputs */
		vhdl << tab << "begin" << endl;
		vhdl << tab << tab << "-- Send reset" <<endl;
		vhdl << tab << tab << "rst <= '1';" << endl;
		vhdl << tab << tab << "wait for 10 ns;" << endl;
		vhdl << tab << tab << "rst <= '0';" << endl;

		vhdl << tab << tab << "while not endfile(inputsFile) loop" << endl;
		vhdl << tab << tab << tab << "readline(inputsFile, input);" << endl;
		vhdl << tab << tab << tab << "readline(inputsFile, expectedOutput); -- unused in this process" << endl;

		// input reading and forwarding to the operator
		for(unsigned int i=0; i < inputSignalVector.size(); i++){
			Signal* s = inputSignalVector[i];
			vhdl << tab << tab << tab << "read(input ,V_"<< s->getName() << ");" << endl;
			vhdl << tab << tab << tab << "read(input,tmpChar);" << endl; // we consume the character between each inputs
			if ((s->width() == 1) && (!s->isBus())) vhdl << tab << tab << tab << s->getName() << " <= to_stdlogicvector(V_" << s->getName() << ")(0);" << endl;
			else vhdl << tab << tab << tab << s->getName() << " <= to_stdlogicvector(V_" << s->getName() << ");" << endl;
			// adding the IO to IOorder
			IOorderInput.push_back(s->getName());
		}
		vhdl << tab << tab << tab << "wait for 10 ns;" << endl; 
		vhdl << tab << tab << "end loop;" << endl;
		vhdl << tab << tab << tab << "wait for "<< op->getPipelineDepth()*10+100 <<" ns; -- wait for pipeline to flush (and some more)" << endl;
		vhdl << tab << "end process;" << endl;
		vhdl << endl;

		/**
		 * Declaration of a waiting time between sending the input and comparing
		 * the result with the output
		 * in case of a pipelined operator we have to wait the complete latency of all the operator
		 * that means all the pipeline stages each step
		 * TODO : entrelaced the inputs / outputs in order to avoid this wait
		 */
		vhdl << tab << " -- Process that verifies the corresponding output" << endl;
		vhdl << tab << "process" << endl;
		/* Variable declaration */
		vhdl << tab << tab << "file inputsFile : text is \"test.input\"; " << endl; // declaration of the input file
		vhdl << tab << tab << "variable input, expectedOutput : line; " << endl;  // variables to read a line
		vhdl << tab << tab << "variable testCounter : integer := 1;" << endl;
		vhdl << tab << tab << "variable errorCounter : integer := 0;" << endl;
		vhdl << tab << tab << "variable expectedOutputLength : integer;" << endl;
		vhdl << tab << tab << "variable expectedOutputString : string(1 to 10000);" << endl;

#if 0
		/* Variable to store value for inputs and expected outputs*/
		for(Signal* s: outputSignalVector){
			vhdl << tab << tab << "variable V_" << s->getName();
			if ((s->width() != 1) || (s->isBus())) vhdl << " : bit_vector("<< s->width() - 1 << " downto 0);" << endl;
			else  vhdl << " : bit;" << endl;
			//			vhdl << tab << tab << "variable expected_"  << s->getName() << ": string (1 to 10000);" << endl; // will be a copy of inline
			//vhdl << tab << tab << "variable expected_size_"  << s->getName() << " : integer;" << endl;
			vhdl << tab << tab << "type valueArray_"  << s->getName() << " is array(1 to 10) of bit_vector(" << s->width()-1 << " downto 0);" << endl;
			vhdl << tab << tab << "variable expected_"  << s->getName() << " : valueArray_"  << s->getName() << ";" << endl;
			vhdl << tab << tab << "variable testSuccess_"  << s->getName() << " : boolean;" << endl;
		}
#endif
		vhdl << tab << tab << "variable testSuccess: boolean;" << endl;
		vhdl << tab << "begin" << endl;
		vhdl << tab << tab << "wait for 12 ns; -- wait for reset " << endl; 
		if (op->getPipelineDepth() > 0){
			vhdl << tab << tab << "wait for "<< op->getPipelineDepth()*10 <<" ns; -- wait for pipeline to flush" <<endl;
		};

		vhdl << tab << tab << "while not endfile(inputsFile) loop" << endl;
		vhdl << tab << tab << tab << "readline(inputsFile, input); -- unused" << endl; // read the input line
		vhdl << tab << tab << tab << "readline(inputsFile, expectedOutput);" << endl; // read the outputs line
		vhdl << tab << tab << tab << "expectedOutputLength := expectedOutput'Length;" << endl;
		vhdl << tab << tab << tab << "expectedOutputString := expectedOutput.all & (expectedOutputLength+1 to 10000 => ' ');" << endl;
		vhdl << tab << tab << tab << "testSuccess := testLine(testCounter, expectedOutputString";
		for(Signal* s: outputSignalVector){
			vhdl << ", " << s->getName();
			IOorderOutput.push_back(s->getName());

		}
		vhdl << 	");" << endl;
		vhdl << tab << tab << tab << "if not testSuccess" << " then " << endl;
		vhdl << tab << tab << tab << tab << tab << "report(\"Test number \" & integer'image(testCounter) & \", incorrect output  \" & lf & "
				 << "\" expected values:\" & expectedOutputString(1 to expectedOutputLength)";
#if 0
		vhdl << " & lf & \"          result: \" & str(" << s->getName() <<") ) severity error;"<< endl;

		vhdl << " & lf & \"          result: \" & str(" << s->getName() <<") ) severity error;"<< endl;
#endif
		vhdl << ") severity error;"<< endl;
		vhdl << tab << tab << tab << tab << tab << "errorCounter := errorCounter + 1; -- incrementing global error counter" << endl;
		vhdl << tab << tab << tab << "end if;" << endl;
		vhdl << tab << tab << tab << tab << "testCounter := testCounter + 1; -- incrementing global error counter" << endl;
			
		/*
		for(Signal* s: outputSignalVector){
			vhdl << tab << tab << tab << "read(expectedOutput, possibilityNumber);" << endl;


vhdl << tab << tab << tab << "if possibilityNumber = -1 then -- this means an interval" << endl;
			vhdl << tab << tab << tab << "  furtherRead := 2;" << endl;
			vhdl << tab << tab << tab << "else " << endl;
			vhdl << tab << tab << tab << "  furtherRead := possibilityNumber;" << endl;			
			vhdl << tab << tab << tab << "end if;" << endl;
			vhdl << tab << tab << tab << "if possibilityNumber = 0 then" << endl;
			vhdl << tab << tab << tab << "  furtherRead := 0;" << endl;						
			vhdl << tab << tab << tab << "  -- TODO define the semantics of lack of expected output" << endl; 
			vhdl << tab << tab << tab << "else " << endl;			
			vhdl << tab << tab << tab << tab << "for i in 1 to furtherRead loop " << endl;
			vhdl << tab << tab << tab << tab << tab << "read(expectedOutput, expected_"  << s->getName() << "(i));" << endl;
			vhdl << tab << tab << tab << tab << "end loop;" << endl;
			vhdl << tab << tab << tab << "end if;" << endl;

			//vhdl << oneTestVHDL(s, tab+tab+tab);

			IOorderOutput.push_back(s->getName());
		};
		*/
		vhdl << tab << tab << tab << "wait for 10 ns;" << endl;
		vhdl << tab << tab << "end loop;" << endl;
		vhdl << tab << tab << "report integer'image(errorCounter) & \" error(s) encoutered.\" severity note;" << endl;
		vhdl << tab << tab << "report \"End of simulation after \" & integer'image(testCounter) & \" tests\" severity note;" <<endl;
		vhdl << tab << "end process;" <<endl;


		
		/* Generating a file of inputs */
		// opening a file to write down the output (for text-file based test)
		// if n < 0 we do not generate a file
		if (n >= 0) {
			string inputFileName = "test.input";
			ofstream fileOut(inputFileName.c_str(),ios::out);
			// if error at opening, let's mention it !
			if (!fileOut) {
				ostringstream e;
				e << "FloPoCo was not able to open " << inputFileName << " in order to write inputs. " << endl;
				throw e.str();
			}
			for (int i = 0; i < tcl.getNumberOfTestCases(); i++)	{
				TestCase* tc = tcl.getTestCase(i);
				fileOut << tc->generateInputString(IOorderInput, IOorderOutput);
			}

			// closing input file
			fileOut.close();
		};

	}


	void TestBench::generateTestInVhdl() {
		vhdl << tab << "-- Setting the inputs" <<endl;
		vhdl << tab << "process" <<endl;
		vhdl << tab << "begin" <<endl;
		vhdl << tab << tab << "-- Send reset" <<endl;
		vhdl << tab << tab << "rst <= '1';" << endl;
		vhdl << tab << tab << "wait for 10 ns;" << endl;
		vhdl << tab << tab << "rst <= '0';" << endl;
		for (int i = 0; i < tcl.getNumberOfTestCases(); i++){
			vhdl << tcl.getTestCase(i)->getInputVHDL(tab + tab);
			vhdl << tab << tab << "wait for 10 ns;" <<endl;
		}
		vhdl << tab << tab << "wait for 100000 ns; -- allow simulation to finish" << endl;
		vhdl << tab << "end process;" <<endl;
		vhdl <<endl;

		int currentOutputTime = 0;
		vhdl << tab << "-- Checking the outputs" <<endl;
		vhdl << tab << "process" <<endl;
		vhdl << tab << "begin" <<endl;
		vhdl << tab << tab << "wait for 12 ns; -- wait for reset to complete" <<endl;
		currentOutputTime += 12;
		if (op->getPipelineDepth() > 0){
			vhdl << tab << tab << "wait for "<< op->getPipelineDepth()*10 <<" ns; -- wait for pipeline to flush" <<endl;
			currentOutputTime += op->getPipelineDepth()*10;
		}


		for (int i = 0; i < tcl.getNumberOfTestCases(); i++) {
			vhdl << tab << tab << "-- current time: " << currentOutputTime <<endl;
			TestCase* tc = tcl.getTestCase(i);
			if (tc->getComment() != "")
				vhdl << tab <<  "-- " << tc->getComment() << endl;
			vhdl << tc->getInputVHDL(tab + tab + "-- input: "); // this method is in TestCase.cpp
			vhdl << tc->getExpectedOutputVHDL(tab + tab); // this method is in TestCase.cpp
			vhdl << tab << tab << "wait for 10 ns;" <<endl;
			currentOutputTime += 10;
		}


		vhdl << tab << tab << "report \"End of simulation\" severity note;" <<endl;
		vhdl << tab << "end process;" <<endl;

	}

	TestBench::~TestBench() {
	}




	void TestBench::outputVHDL(ostream& o, string name) {
		licence(o,"Florent de Dinechin, Cristian Klein, Nicolas Brunie (2007-2010)");

		Operator::stdLibs(o);

		outputVHDLEntity(o);
		o << "architecture behavorial of " << name  << " is" << endl;

		// the operator to wrap
		op->outputVHDLComponent(o);
		// The local signals
		o << buildVHDLSignalDeclarations();

		o << endl;

		/* Generation of Vhdl function to parse file into std_logic_vector */
		o << " -- converts std_logic into a character" << endl;
		o << tab << "function chr(sl: std_logic) return character is" << endl
		  << tab << tab << "variable c: character;" << endl
		  << tab << "begin" << endl
		  << tab << tab << "case sl is" << endl
		  << tab << tab << tab << "when 'U' => c:= 'U';" << endl
		  << tab << tab << tab << "when 'X' => c:= 'X';" << endl
		  << tab << tab << tab << "when '0' => c:= '0';" << endl
		  << tab << tab << tab << "when '1' => c:= '1';" << endl
		  << tab << tab << tab << "when 'Z' => c:= 'Z';" << endl
		  << tab << tab << tab << "when 'W' => c:= 'W';" << endl
		  << tab << tab << tab << "when 'L' => c:= 'L';" << endl
		  << tab << tab << tab << "when 'H' => c:= 'H';" << endl
		  << tab << tab << tab << "when '-' => c:= '-';" << endl
		  << tab << tab << "end case;" << endl
		  << tab << tab << "return c;" << endl
		  << tab <<  "end chr;" << endl;
		o << endl;

		o << tab << "-- converts bit to std_logic (1 to 1)" << endl
			<<	tab << "function to_stdlogic(b : bit) return std_logic is" << endl
			<< tab << tab << " variable sl : std_logic;" << endl
			<< tab << "begin" << endl
			<< tab << tab << "case b is " << endl
			<< tab << tab << tab << "when '0' => sl := '0';" << endl
			<< tab << tab << tab << "when '1' => sl := '1';" << endl
			<< tab << tab << "end case;" << endl
			<< tab << tab << "return sl;" << endl
			<< tab << "end to_stdlogic;" << endl;
		o << endl;

		o << tab << "-- converts std_logic into a string (1 to 1)" << endl
		  << tab << "function str(sl: std_logic) return string is" << endl
		  << tab << " variable s: string(1 to 1);" << endl
		  << tab << " begin" << endl
		  << tab << tab << "s(1) := chr(sl);" << endl
		  << tab << tab << "return s;" << endl
		  << tab << "end str;" << endl;
		o << endl;
		
		o << tab << "-- converts std_logic_vector into a string (binary base)" << endl
			<< tab << "-- (this also takes care of the fact that the range of" << endl
		  << tab << "--  a string is natural while a std_logic_vector may" << endl
		  << tab << "--  have an integer range)" << endl
		  << tab << "function str(slv: std_logic_vector) return string is" << endl
		  << tab << tab << "variable result : string (1 to slv'length);" << endl
		  << tab << tab << "variable r : integer;" << endl
		  << tab << "begin" << endl
		  << tab << tab << "r := 1;" << endl
		  << tab << tab << "for i in slv'range loop" << endl
		  << tab << tab << tab << "result(r) := chr(slv(i));" << endl
		  << tab << tab << tab << "r := r + 1;" << endl
		  << tab << tab << "end loop;" << endl
		  << tab << tab << "return result;" << endl
		  << tab << "end str;" << endl;
		o << endl;

		if(hasFPOutputs){
			o << tab << "-- FP compare function (found vs. real)\n" <<
				tab << "function fp_equal(a : std_logic_vector; b : std_logic_vector) return boolean is\n" <<
				tab << "begin\n" <<
				tab << tab << "if b(b'high downto b'high-1) = \"01\" then\n" <<
				tab << tab << tab << "return a = b;\n" <<
				tab << tab << "elsif b(b'high downto b'high-1) = \"11\" then\n" <<
				tab << tab << tab << "return (a(a'high downto a'high-1)=b(b'high downto b'high-1));\n" <<
				tab << tab << "else\n" <<
				tab << tab << tab << "return a(a'high downto a'high-2) = b(b'high downto b'high-2);\n" <<
				tab << tab << "end if;\n" <<
				tab << "end;\n";
			o << endl;
		}

		
		if(hasIEEEOutputs){
			/* If op is an IEEE operator (IEEE input and output, we define) the function
			 * fp_equal for the considered precision in the ieee case
			 */
			o << tab << "-- test isZero\n" <<
				tab << "function iszero(a : std_logic_vector) return boolean is\n" <<
				tab << "begin\n" <<
				tab << tab << "return  a = (a'high downto 0 => '0');\n" <<        // test if exponent = "0000---000"
				tab << "end;\n";
			o << endl;
			
			o <<	tab << "-- FP IEEE compare function (found vs. real)\n" <<
				tab << "function fp_equal_ieee(a : std_logic_vector;"
				<< " b : std_logic_vector;"
				<< " we : integer;"
				<< " wf : integer) return boolean is\n"
				<<	tab << "begin\n" <<
				tab << tab << "if a(wf+we downto wf) = b(wf+we downto wf) and b(we+wf-1 downto wf) = (we downto 1 => '1') then\n" <<        // test if exponent = "1111---111"
				tab << tab << tab << "if iszero(b(wf-1 downto 0)) then return  iszero(a(wf-1 downto 0));\n" <<               // +/- infinity cases
				tab << tab << tab << "else return not iszero(a(wf - 1 downto 0));\n" <<
				tab << tab << tab << "end if;\n" <<
				tab << tab << "else\n" <<
				tab << tab << tab << "return a(a'high downto 0) = b(b'high downto 0);\n" <<
				tab << tab << "end if;\n" <<
				tab << "end;\n";
			o << endl;
		}
		/* In VHDL, literals may be incorrectly converted to „std_logic_vector(... to ...)” instead
		 * of „downto”. So, for each FP output width, create a subtype used for casting.
		 */
		{
			std::set<int> widths;
			for (int i=0; i<op->getIOList().size(); i++){
				Signal* s = op->getIOListSignal(i);

				if (s->type() != Signal::out) continue;
				if (s->isFP() != true and s->isIEEE() != true) continue;
				//				std::cout << "debug : inserting size " << s->width() << " in fp subtype map. " << std::endl;
				widths.insert(s->width());
			}

			if (widths.size() > 0)
				o << tab << "-- FP subtypes for casting\n";
			for (std::set<int>::iterator it = widths.begin(); it != widths.end(); it++)
				{
					int w = *it;
					o << tab << "subtype fp" << w << " is std_logic_vector(" << w-1 << " downto 0);\n";
				}
		}

		// thefunction that actually implements the test
		generateLineTestFunction(o);

		o << endl;

		o << "begin\n";
		 
		o	 << tab << "-- Ticking clock signal" <<endl;
		o	 << tab << "process" <<endl;
		o	 << tab << "begin" <<endl;
		o	 << tab << tab << "clk <= '0';" <<endl;
		o	 << tab << tab << "wait for 5 ns;" <<endl;
		o	 << tab << tab << "clk <= '1';" <<endl;
		o	 << tab << tab << "wait for 5 ns;" <<endl;
		o	 << tab << "end process;" <<endl;
		o	 << endl;
		// reset is managed when 

		//if the outputs of the tested operator are not synchronized
		//	then registering and delaying the outputs might be necessary
		bool opHasOutputsDesync = false;

		for(unsigned i=0; i<op->ioList_.size(); i++)
		{
			Signal *s = op->getIOListSignal(i);

			if(s->type() != Signal::out)
				continue;

			for(unsigned j=0; j<op->ioList_.size(); j++)
			{
				Signal *t = op->getIOListSignal(j);

				if((t->type() == Signal::out) && (s->getName() != t->getName()) && (s->getCycle() != t->getCycle())){
					opHasOutputsDesync = true;
					break;
				}
			}

			if(opHasOutputsDesync)
				break;
		}
		if(opHasOutputsDesync == true)
			o << buildVHDLRegisters() << endl;

		//output the actual code of the testbench
		o << vhdl.str() << endl;

		o << "end architecture;" << endl << endl;


	}


		/** Return the total simulation time*/
		int TestBench::getSimulationTime(){
			//			cerr << endl << endl<< simulationTime << endl << endl ;
			return simulationTime;
		}





	OperatorPtr TestBench::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int n;
		bool file;

		if(ui.globalOpList.empty()){
			throw(string("TestBench has no operator to wrap (it should come after the operator it wraps)"));
		}

		ui.parseInt(args, "n", &n);
		ui.parseBoolean(args, "file", &file);
		Operator* toWrap = ui.globalOpList.back();
		Operator* newOp = new TestBench(target, toWrap, n, file);
		// the instance in newOp has added toWrap as a subcomponent of newOp,
		// so we may remove it from globalOpList
		//UserInterface::globalOpList.popback();
		return newOp;
	}

	template <>
	const OperatorDescription<TestBench> op_descriptor<TestBench> {
	    "TestBench", // name
	    "Behavorial test bench for the preceding operator.",
	    "TestBenches", // categories
	    "", // seeAlso
	    "n(int)=-2: number of random tests. If n=-2, an exhaustive test is generated (use only for small operators);\
         file(bool)=true:Inputs and outputs are stored in file test.input (faster). If false, they are stored in the VHDL (easier to read);",
	    ""};
}
