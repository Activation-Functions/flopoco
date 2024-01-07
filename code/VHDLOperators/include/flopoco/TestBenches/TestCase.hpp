#ifndef __TESTCASE_HPP
#define __TESTCASE_HPP

#include <list>
#include <map>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>

#include "flopoco/Signal.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"
#include "flopoco/TestBenches/IEEENumber.hpp"

namespace flopoco{

	/**
		 A test case is a mapping between I/O signal names and boolean values
		 given as mpz.

		 The signal names must be those of Operator->iolist_.

		 The emulate() function of Operator takes a partial test case (mapping
		 all the inputs) and completes it by mapping the outputs.
		 * @see TestBench
		 * @see Operator
		 */

	class Operator;
	class TestCaseList;
	
	
	class TestCase {
	public:

		typedef enum {
			list_of_values=0,          /**< good old technique */
			unsigned_interval=-1,          /**<  */
			signed_interval=-2,          /**<  */
			IEEE_interval=-3,          /**<  */
			floating_point_interval=-4          /**<  */
		} OutputType;

		/** Creates an empty TestCase for operator op */
		TestCase(Operator* op);
		~TestCase();

		/**
		 * Adds an input for this TestCase
		 * @param s The signal which will value the given value
		 * @param v The value which will be assigned to the signal
		 */
		void addInput(std::string s, mpz_class v);

		/**
		 * Adds an input in the FloPoCo FP format for this TestCase
		 * @param s The signal which will value the given value
		 * @param v The value which will be assigned to the signal
		 */
		void addFPInput(std::string s, FPNumber::SpecialValue v);

		/**
		 * Adds an input in the FloPoCo FP format for this TestCase
		 * @param s The signal which will value the given value
		 * @param x the value which will be assigned to the signal, provided as a double.
		 * (use this method with care, typically only for values such as 1.0 which are representable whatever wE or wF)
		 */
		void addFPInput(std::string s, double x);

		/**
		 * Adds an input in the FloPoCo FP format for this TestCase
		 * @param s The signal which will value the given value
		 * @param x the value which will be assigned to the signal, provided as an FPNumber.
		 */
		void addFPInput(std::string s, FPNumber *x);

		/**
		 * Adds an input in the IEEE FP format for this TestCase
		 * @param s The signal which will value the given value
		 * @param v The value which will be assigned to the signal
		 */
		void addIEEEInput(std::string s, IEEENumber::SpecialValue v);

		/**
		 * Adds an input in the IEEE FP format for this TestCase
		 * @param s The signal which will value the given value
		 * @param x the value which will be assigned to the signal, provided as a double.
		 * (use this method with care)
		 */
		void addIEEEInput(std::string s, double x);

		void addIEEEInput(std::string s, IEEENumber ieeeNumber);

		/**
		 * recover the mpz associated to an input
		 * @param s The name of the input
		 */
		mpz_class getInputValue(std::string s);

		void setInputValue(std::string s, mpz_class v);

		/**
		 * Adds an expected output for this signal
		 * @param s The signal for which to assign an expected output
		 * @param v One possible value which the signal might have
		 * @param isSigned if true, v is considered a signed integer, unsigned integer otherwise.
		 */
		void addExpectedOutput(std::string s, mpz_class v, bool isSigned=false);

		/**
		 * Adds an expected output interval for this signal (both endpoints included)
		 * @param s The signal for which to assign an expected output
		 * @param vinf the smallest possible value which the signal might take
		 * @param vsup the largest possible value which the signal might take
		 * @param type type of the data, needed since we need to perform comparisons to check that output is within expected interval 
		 */
		void addExpectedOutputInterval(std::string s, mpz_class vinf, mpz_class vsup, OutputType type=unsigned_interval);

		/**
		 * returns all mpz associated to an output as a vector of mpz_class (added before by addExpectedOutput())
		 * @param s The name of the output
		 */
		std::vector<mpz_class> getExpectedOutputValues(std::string s);
		

		/**
		 * Adds a comment to the output VHDL. "--" are automatically prepended.
		 * @param c Comment to add.
		 */
		void addComment(std::string c);

		/**
		 * Generates the VHDL code necessary for assigning the input signals.
		 * @param prepend A string to prepend to each line.
		 * @return A multi-line VHDL code.
		 */
		std::string getInputVHDL(std::string prepend = "");

		/**
		 * Return the comment string for this test case
		 * @return A single-line VHDL code.
		 */
		std::string getComment();
		
		void setId(int id);

		int getId();
		
		/**
		 * generate a 4-line string: first two lines are the input vector, following two lines are the vector of expected outputs
		 * An expected output is either defined by a positive integer n and a list of n acceptable outputs, 
		   or by a negative integer n and two extremal acceptable outputs that define an acceptable interval.
			 The comparison to use is defined by the OutputType enum above.
		 */
		std::string testFileString(std::list<std::string> inputSignalNames, std::list<std::string> outputSignalNames);


		std::string getDescription();

	private:

		Operator *op_;                       /**< The operator for which this test case is being built */
		std::map<std::string, mpz_class>          inputs; /**< map of signal names to bit vectors */
		std::map<std::string, std::vector<mpz_class> >   outputs; /**< map of signal names to expected outputs. */
		std::map<std::string, OutputType >  outputType; /**< map of signal names to the type of expected outputs (list of values, or intervals with their types) */

		int intId; 
		std::string comment;  /**< an optional comment */
	};



	/**
	 * Represents a list of test cases 
	 */
	class TestCaseList {
	public:
		/**
		 * Creates an empty TestCaseList
		 * @see TestCase
		 */
		TestCaseList();

		/** Destructor */
		~TestCaseList();

		/**
		 * Adds a TestCase to this TestCaseList.
		 * @param t TestCase to add
		 */
		void add(TestCase* t);

		/**
		 * Adds all the TestCases from another TestCaseList to this TestCaseList.
		 * @param t TestCase to add
		 */
		void add(TestCaseList* tcl);

		/**
		 * Get the number of TestCase-es contained in this TestCaseList.
		 * @return The number of TestCase-es
		 */
		int getNumberOfTestCases();

		/**
		 * Get a specific TestCase.
		 * @param i Which TestCase to return
		 * @return The i-th TestCase.
		 */
		TestCase * getTestCase(int i);

	private:
		/** Stores the TestCase */
		std::vector<TestCase*>  v;
		std::map<int,TestCase*> mapCase;
		/* id given to the last registered test case*/

	};

}
#endif
