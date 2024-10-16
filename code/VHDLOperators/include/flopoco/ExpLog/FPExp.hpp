/*
  An FP exponential for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/
#ifndef __FPEXP_HPP
#define __FPEXP_HPP
#include <sstream>
#include <vector>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/Tables/DualTable.hpp"
#include "flopoco/Tables/Table.hpp"
#include "flopoco/ExpLog/ExpArchitecture.hpp"
#include "flopoco/ExpLog/Exp.hpp"

class Fragment;


namespace flopoco{


	class FPExp : public Operator
	{
	public:

		/** @brief The constructor with manual control of all options
		* @param wE exponent size
		* @param wF fraction size
		* @param k size of the input to the first table
		* @param d  degree of the polynomial approximation (if k=d=0, the
		* 			constructor tries to compute sensible values)
		* @param guardBits number of gard bits. If -1, a default value (that
		* 				   depends of the size)  is computed inside the constructor.
		* @param fullInput boolean, if true input mantissa is of size wE+wF+1,
		*                  so that  input shift doesn't padd it with 0s (useful
		*                  for FPPow)
		*/
		FPExp(
					OperatorPtr parentOp,
					Target* target,
					int wE,
					int wF,
					int k,
					int d,
					int guardBits=-1,
					bool fullInput=false);

		~FPExp();

		// Overloading the virtual functions of Operator
		// void outputVHDL(std::ostream& o, std::string name);
		void emulate(TestCase * tc);
		void buildStandardTestCases(TestCaseList* tcl);
		TestCase* buildRandomTestCase(int i);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		static TestList unitTest(int testLevel);

	private:
		int wE; /**< Exponent size */
		int wF; /**< Fraction size */
		int k;  /**< Size of the address bits for the first table  */
		int d;  /**< Degree of the polynomial approximation */
		int g;  /**< Number of guard bits */
	};
}
#endif
