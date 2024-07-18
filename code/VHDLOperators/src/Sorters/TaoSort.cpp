// general c++ library for manipulating streams
#include <iostream>
#include <sstream>

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitrary large
   entries */
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"
#include "gmp.h"
#include "mpfr.h"

// include the header of the Operator
#include "flopoco/Sorters/TaoSort.hpp"

using namespace std;
namespace flopoco {

	/* Filling the different fields that are used to 
	*/
	template<>
	const OperatorDescription<TaoSort> op_descriptor<TaoSort> {
		/*Name:         */ "TaoSort",
		/*Description:  */ "Implements a sorting method on unsigned integers that returns the index of how to sort them",
		/*Category:     */ "Sorters", //from the list defined in UserInterface.cpp
		/*See also:     */ "",
		"N(int): Number of elements we are sorting; \
		 wKey(int): Width of the integer we are comparing; \
		 outputDifference(bool)=false: If true, will output the absolute value of the difference between the inputs. Regular user should not need this option;",
		// More documentation for the HTML pages. If you want to link to your blog, it is here.
		    "This is VHDL code following a sort introduced in 'Correctly Rounded Architectures for Floating-Point Multi-Operand Addition and Dot-Product Computation' Yao Tao, Gao Deyuan, Fan Xiaoya, Jari Nurmi. The outputDifference is needed to reproduce the architecture in this paper",
		};

	
	TaoSort::TaoSort(OperatorPtr parentOp, Target* target, int N_, int wKey_, bool outputDifference_) : Operator(parentOp, target), N(N_), wKey(wKey_), outputDifference(outputDifference_) {
		srcFileName="TaoSort";

		// definition of the name of the operator
		ostringstream name;
		name << "TaoSort_" << N << "_" << wKey << "_" << outputDifference;
		setName(name.str()); // See also setNameWithFrequencyAndUID()
		// Copyright 
		setCopyrightString("Oregane Desrentes 2023");
		int wIndex = sizeInBits(N);

		// declaring inputs and outputs
		for (int i =0; i < N; i++) {
			string is = to_string(i);
			addInput ("key" + is, wKey);
			addOutput ("sorted_index" + is, wIndex);
		}
		if (outputDifference) {
			for (int i = 0; i < N; i++) {
				for (int j=i+1; j < N; j++) {
					addOutput("d" + to_string(i) + to_string(j), wKey);
				}
			}

		}

		addFullComment("Start of vhdl generation");
		REPORT(LogLevel::DETAIL,"Declaration of TaoSort \n");

		REPORT(LogLevel::VERBOSE, "this operator has received 3 parameters " << N << ", " << wKey << "and" << "outputDifference");
		REPORT(LogLevel::DEBUG,"debug of TaoSort");


		// Start by comparing all the exponents together
		for (int i = 0; i < N; i++) {
			string is = to_string(i);
			for (int j = i+1; j < N; j++) {
				string js = to_string(j);
				if (outputDifference) {
					vhdl << declare(target->adderDelay(wKey), "e_" + is + + "_" + js, wKey+1) << "<= (\"0\" & key" << i << ") -  (\"0\" & key" << j << ");" << endl; 
					vhdl << declare(target->adderDelay(wKey), "e_" + js + + "_" + is, wKey) << "<= (key" << j << ") -  (key" << i << ");" << endl; 
					//The comparaison result is the carry bit in the substraction
					vhdl << declare(0., "c_"+is+"_"+js, 1) << "<= \"0\" when e_" << i << "_" << j << of(wKey) << "='0' else \"1\";" << endl;
					// Choose the right difference so it is the absolute value
					vhdl <<  "d" << i << j << "<= e_" << i << "_" << j << range(wKey-1, 0)  << " when c_" << i << "_" << j << "=\"0\" else e_" << j << "_" << i<< ";" << endl;
					} else {
					vhdl << declare(target->ltComparatorDelay(wKey), "c_"+is+"_"+js, 1) << "<= \"1\" when key" << i << "< key" << j << " else \"0\";" << endl;
				}
			}
		}

		// compute the index
		
		for (int i = 0; i < N; i++) {
			vhdl << "sorted_index" << i << "<= ";
			for (int j = 0; j < i; j++) {
				vhdl << "(\"" << string(wIndex-1, '0') << "\" & NOT (c_" << j << "_" << i << ")) + ";
			}
			for (int j = i+1; j < N; j++) {
				vhdl << "(\"" << string(wIndex-1, '0') << "\" & c_" << i << "_" << j << ") + ";
			}
			vhdl << "(\"" << string(wIndex, '0') << "\");" << endl;
		}

		addFullComment("End of vhdl generation");
	};

	// For testing, it would be possible to use the 0-1 principle which means that testing all the 0 or 1 combinations
	// of the keys is enough to exhaustively test the sort. The payload should be different so it can test that correctly
	// This could be added in the autotest function
	
	void TaoSort::emulate(TestCase * tc) {
		// We use the Tao sort method to get the right result
		// and then verify that the order is correct, the VHDL will be checked against this.
		mpz_class list_to_sort[N];
		mpz_class sorted_list[N];
		int list_of_indices[N];
		for (int i =0; i < N; i++) {
			string is = to_string(i);
			list_to_sort[i] = tc->getInputValue("key" + is);
		}

		// Reuse the Tao sorting method
		int c_matrix[N][N];
		for (int i=0; i < N; i++) {
			for (int j=0; j < N; j++) {
				c_matrix[i][j] = (list_to_sort[i] >= list_to_sort[j]) ? 0 : 1;
			}
		}

		for (int i = 0; i < N; i++) {
			//cout << " Calcul pour i = " << i << " : ";
			int index = 0;
			for (int j = 0; j < i; j++) {
				index += (1 - c_matrix[j][i]);
			}
			for (int j = i+1; j < N; j++) {
				index += c_matrix[i][j];
			}
			list_of_indices[i] = index;
		}

		for (int i = 0; i < N; i++) {
			sorted_list[list_of_indices[i]] = list_to_sort[i];
		}
		// Check the sort is correct
		for (int i = 0; i < N-1; i++) {
			if (sorted_list[i] < sorted_list[i+1]) {
				for (int j = 0; j < N; j++) {
					cout << j << ", " << sorted_list[j] << endl;
				}
				THROWERROR("Sort does not work for those entries");
				break;
			}
		}

		// Add expected output to check VHDL against correct sort
		for (int i =0; i < N; i++) {
			string is = to_string(i);
			tc->addExpectedOutput ("sorted_index" + is, list_of_indices[i]);
		}

		if (outputDifference) {
			for (int i = 0; i < N; i++) {
				string is = to_string(i);
				for (int j = i+1; j < N; j++) {
					string js = to_string(j);
					tc->addExpectedOutput("d" + is + js, abs(list_to_sort[i] - list_to_sort[j]));
				}
			}
		}
	}


	void TaoSort::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}


	OperatorPtr TaoSort::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int N, wKey;
		bool outputDifference;
		ui.parseInt(args, "N", &N);
		ui.parseInt(args, "wKey", &wKey); 
		ui.parseBoolean(args, "outputDifference", &outputDifference); 
		return new TaoSort(parentOp, target, N, wKey, outputDifference);
	}
}//namespace
