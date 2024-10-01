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
#include "flopoco/Sorters/SortingNetwork.hpp"

using namespace std;
namespace flopoco {

	/* Filling the different fields that are used to 
	*/
	template<>
	const OperatorDescription<SortingNetwork> op_descriptor<SortingNetwork> {
		/*Name:         */ "SortingNetwork",
		/*Description:  */ "Implements a sorting network on unsigned integers with payload",
		/*Category:     */ "Sorters", //from the list defined in UserInterface.cpp
		/*See also:     */ "",
		"N(int): Number of elements we are sorting; \
         wKey(int): Width of the integer we are comparing; \
         wPayload(int): Width of the payload. Can be 0 for no payload; \
	 direction(bool): Sorts increasing if true and decreasing if false; ",
		// More documentation for the HTML pages. If you want to link to your blog, it is here.
	    "This is VHDL code following the bitonic sort, but can support different sorts if needed.",
		};

	
	SortingNetwork::SortingNetwork(OperatorPtr parentOp, Target* target, int N_, int wKey_, int wPayload_, bool direction_) : Operator(parentOp, target), N(N_), wKey(wKey_), wPayload(wPayload_), direction(direction_) {
		srcFileName="SortingNetwork";

		// definition of the name of the operator
		ostringstream name;
		name << "SortingNetwork_" << N << "_" << wKey << "_" << wPayload;
		setName(name.str()); // See also setNameWithFrequencyAndUID()
		// Copyright 
		setCopyrightString("Oregane Desrentes 2023");

		// declaring inputs and outputs
		for (int i =0; i < N; i++) {
			string is = to_string(i);
			addInput ("key" + is + "_0", wKey);
			addOutput ("sorted_key" + is, wKey);
			if (wPayload != 0) {
				addInput ("payload" + is + "_0", wPayload);
				addOutput ("sorted_payload" + is, wPayload);
			}
		}

		addFullComment("Start of vhdl generation");
		REPORT(LogLevel::DETAIL,"Declaration of SortingNetwork \n");
		REPORT(LogLevel::DETAIL,"For some reason the pipelining of this operator does not work \n");

		REPORT(LogLevel::VERBOSE, "this operator has received 3 parameters " << N << ", " << wKey << " and " << wPayload);
		REPORT(LogLevel::DEBUG,"debug of SortingNetwork");

		// Start with getting the sort we plan to use
		// If the optimal sort is available, use the optimal one optimal_available
		OptimalDepthSort *osort;
		osort = new OptimalDepthSort(N_);
		BitonicSort *bsort;
		bsort = new BitonicSort(N_);
		if (osort->flat_sort.empty()) {
			flat_sort = bsort->flat_sort;
		} else {
			flat_sort = osort->flat_sort;
		}

		int* nameid = (int*)malloc(N * sizeof(int));
  		memset(nameid, 0, N * sizeof(int));

		string first_id, second_id, first_id_after, second_id_after;
		for (auto swap : flat_sort) {
			// swap is a pair, if first > second then swap
			// The changing numbers
			first_id = to_string(swap.first) + "_" + to_string(nameid[swap.first]);
			second_id = to_string(swap.second) + "_" + to_string(nameid[swap.second]);
			first_id_after = to_string(swap.first) + "_" + to_string(nameid[swap.first]+1);
			second_id_after = to_string(swap.second) + "_" + to_string(nameid[swap.second]+1);
			/* //IntComparator is not needed here
			newInstance("IntComparator",
				"Cmp_"+first_id+"_"+second_id,
				"w="+to_string(wKey) + " flags=4", // 4 because we just want to know if X bigger than Y
				"X=>key"+first_id+",Y=>key"+second_id,
				"XgtY=>swap_"+first_id+"_"+second_id);
			*/
			// The next line is what the IntComparator does for small numbers
			vhdl << declare(target->ltComparatorDelay(wKey), "swap_"+first_id+"_"+second_id, 1, false) << "<= '1' when key" << first_id << "> key" << second_id << " else '0';" << endl;
			// Swap after Comparaison
			addComment("Swapping the inputs if " + to_string(swap.first) +" is bigger than " + to_string(swap.second));
			// swap comparing number
			vhdl << declare(target->logicDelay(1), "key" + first_id_after, wKey) << " <= key" << second_id << " when swap_" << first_id << "_" << second_id << "='1' else key" << first_id << ";" << endl;
			vhdl << declare(target->logicDelay(1), "key" + second_id_after, wKey) << " <= key" << first_id << " when swap_" << first_id << "_" << second_id << "='1' else key" << second_id << ";" << endl;
			// swap payload
			if (wPayload != 0) {
				vhdl << declare(target->logicDelay(1), "payload" + first_id_after, wPayload) << " <= payload" << second_id << " when swap_" << first_id << "_" << second_id << "='1' else payload" << first_id << ";" << endl;
				vhdl << declare(target->logicDelay(1), "payload" + second_id_after, wPayload) << " <= payload" << first_id << " when swap_" << first_id << "_" << second_id << "='1' else payload" << second_id << ";" << endl;
			}
			// add the id
			nameid[swap.first]++;
			nameid[swap.second]++;
		}

		// now map everything on the output
		if (direction) { // sort increasing
			for (int i =0; i < N; i++) {
				vhdl << "sorted_key" << i << " <= key" << i << "_" << nameid[i] << ";" << endl;
				if (wPayload != 0) {
					vhdl << "sorted_payload" << i << " <= payload" << i << "_" << nameid[i] << ";" << endl;
				}
			}
		} else {
			for (int i =0; i < N; i++) {
				vhdl << "sorted_key" << N-1-i << " <= key" << i << "_" << nameid[i] << ";" << endl;
				if (wPayload != 0) {
					vhdl << "sorted_payload" << N-1-i << " <= payload" << i << "_" << nameid[i] << ";" << endl;
				}
			}
		}


		addFullComment("End of vhdl generation");
	};

	// For testing, it would be possible to use the 0-1 principle which means that testing all the 0 or 1 combinations
	// of the keys is enough to exhaustively test the sort. The payload should be different so it can test that correctly
	// This could be added in the autotest function
	
	void SortingNetwork::emulate(TestCase * tc) {
		// the bitonic sort is not stable, so have to use the bitonic sort method to get the right result
		// and then verify that the order is correct, the VHDL will be checked against this.
		pair<mpz_class, mpz_class> list_to_sort[N];
		pair<mpz_class, mpz_class> val;
		for (int i =0; i < N; i++) {
			string is = to_string(i);
			if (wPayload != 0) {
				val = std::make_pair(tc->getInputValue("key" + is + "_0"), tc->getInputValue("payload" + is + "_0"));
			} else {
				val = std::make_pair(tc->getInputValue("key" + is + "_0"), 0);
			}
			list_to_sort[i] = val;
		}

		// Reuse the sort from BitonicSort
		for (auto swap : flat_sort) { // swap is a pair, if first > second then swap
			if (list_to_sort[swap.first].first > list_to_sort[swap.second].first) {
				val = list_to_sort[swap.first];
				list_to_sort[swap.first] = list_to_sort[swap.second];
				list_to_sort[swap.second] = val;
			}
		}

		// Check the sort is correct
		for (int i = 0; i < N-1; i++) {
			if (list_to_sort[i].first > list_to_sort[i+1].first) {
				for (int j = 0; j < N; j++) {
					cerr << j << ", " << (list_to_sort[j].first) << endl;
				}
				THROWERROR("Sort does not work for those entries");
				break;
			}
		}

		// Add expected output to check VHDL against correct sort
		if (direction) {
			for (int i =0; i < N; i++) {
				string is = to_string(i);
				tc->addExpectedOutput ("sorted_key" + is, list_to_sort[i].first);
				if (wPayload != 0) {
					tc->addExpectedOutput ("sorted_payload" + is, list_to_sort[i].second);
				}
			}
		} else {
			for (int i =0; i < N; i++) {
				string is = to_string(N-1-i);
				tc->addExpectedOutput ("sorted_key" + is, list_to_sort[i].first);
				if (wPayload != 0) {
					tc->addExpectedOutput ("sorted_payload" + is, list_to_sort[i].second);
				}
			}
		}
	}


	void SortingNetwork::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}




	OperatorPtr SortingNetwork::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int N, wKey, wPayload;
		bool direction;
		ui.parseInt(args, "N", &N);
		ui.parseInt(args, "wKey", &wKey); 
		ui.parseInt(args, "wPayload", &wPayload); 
		ui.parseBoolean(args, "direction", &direction); 
		return new SortingNetwork(parentOp, target, N, wKey, wPayload, direction);
	}
}//namespace
