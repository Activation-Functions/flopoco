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
#include "flopoco/Sorters/SortWrapper.hpp"

using namespace std;
namespace flopoco {

	/* Filling the different fields that are used to 
	*/
	template<>
	const OperatorDescription<SortWrapper> op_descriptor<SortWrapper> {
		/*Name:         */ "SortWrapper",
		/*Description:  */ "Wrapper for sorting methods, can sort keys with payload",
		/*Category:     */ "Sorters", //from the list defined in UserInterface.cpp
		/*See also:     */ "",
		"N(int): Number of elements we are sorting; \
         wKey(int): Width of the integer we are comparing; \
         wPayload(int): Width of the payload. Can be 0 if no payload is needed; \
	 direction(bool): Sorts increasing if true and decreasing if false; \
	 indexPayload(bool)=false: If true then use indices in the sorting network instead of the payload. This is useful when the payload takes more time to compute than the key, but it is a bit more expensive in area.;\
         method(int): Which sort to use, 0 for sorting network, 1 for Tao sort; ",
		// More documentation for the HTML pages. If you want to link to your blog, it is here.
	    "This is VHDL code is a wrapper for sorters",
		};

	
	SortWrapper::SortWrapper(OperatorPtr parentOp, Target* target, int N_, int wKey_, int wPayload_, bool direction_, bool indexPayload_, int method_) : Operator(parentOp, target), N(N_), wKey(wKey_), wPayload(wPayload_), direction(direction_), indexPayload(indexPayload_), method(method_) {
		srcFileName="SortWrapper";

		// definition of the name of the operator
		ostringstream name;
		name << "SortWrapper_" << N << "_" << wKey << "_" << wPayload << "_" << direction << "_";
		if (method==0) {
			name << "SortingNetwork";
		} else {
			name << "TaoSort";
		}
		setName(name.str()); // See also setNameWithFrequencyAndUID()
		// Copyright 
		setCopyrightString("Oregane Desrentes 2023");

		// declaring inputs and outputs
		for (int i =0; i < N; i++) {
			string is = to_string(i);
			addInput ("key" + is + "_i", wKey);
			addOutput ("sorted_key" + is + "_o", wKey);
			if (wPayload != 0) {
				addInput ("payload" + is + "_i", wPayload);
				addOutput ("sorted_payload" + is + "_o", wPayload);
			}
		}

		addFullComment("Start of vhdl generation");
		REPORT(LogLevel::DETAIL,"Declaration of SortWrapper \n");
		
		// link to other sort, as if this payload is mantissa and other payload is index
		// No payload for tao sort, retrieve sorted key too		
		

		ostringstream param, inmap, outmap;
		param << "N=" << N;
		param << " wKey=" << wKey;

		int wIndex = sizeInBits(N);

		if (method==0) { // Sorting network
			if (indexPayload && wPayload != 0) { // Using indices as payload and multiplexing afterwards. This is not needed if there is no payload	
				param << " wPayload=" << wIndex; 
				param << " direction=" << direction;
				
				// get indices
				for (int i =0; i < N; i++) {
					vhdl << tab << declare(.0, "index"+to_string(i), wIndex) << "<= \""<< unsignedBinary(i, wIndex) << "\";" << endl;
				}

				inmap << "key0_0" << "=>" << "key0_i";
				inmap << ",payload0_0" << "=>" << "index0";
				outmap << "sorted_key0" << "=>" << "sorted_key0_o";
				outmap << ",sorted_payload0" << "=>" << "sorted_index0";
				for (int i =1; i < N; i++) {
					inmap << ",key" << i << "_0" << "=>" << "key" << i << "_i";
					inmap << ",payload" << i << "_0" << "=>" << "index" << i;
					outmap << ",sorted_key" << i << "=>" << "sorted_key" << i << "_o";
					outmap << ",sorted_payload" << i << "=>" << "sorted_index" << i;
				}
				cout << param.str() <<endl;

				OperatorPtr op = newInstance("SortingNetwork", "sort_network", param.str(), inmap.str(), outmap.str());
				SortingNetwork* op_sort = dynamic_cast<SortingNetwork*>(op);
				sort_used = op_sort->flat_sort;
			
				// mux payload
				for (int i =0; i < N; i++) {
					vhdl << "sorted_payload" << i << "_o <="; 
					for (int j = 0; j < N; j++) {
						vhdl << tab << "payload" << j << "_i when sorted_index" << i << "= \"" << unsignedBinary(j, wIndex) << "\" else " << endl;
					}
					vhdl << tab << "\"" << string(wPayload, '-') << "\";" << endl;
				}
			} else {
				param << " wPayload=" << wPayload; 
				param << " direction=" << direction;
				
				inmap << "key0_0" << "=>" << "key0_i";
				outmap << "sorted_key0" << "=>" << "sorted_key0_o";
				if (wPayload != 0) {
					inmap << ",payload0_0" << "=>" << "payload0_i";
					outmap << ",sorted_payload0" << "=>" << "sorted_payload0_o";
				}
				for (int i =1; i < N; i++) {
					inmap << ",key" << i << "_0" << "=>" << "key" << i << "_i";
					outmap << ",sorted_key" << i << "=>" << "sorted_key" << i << "_o";
					if (wPayload != 0) {
						inmap << ",payload" << i << "_0" << "=>" << "payload" << i << "_i";
						outmap << ",sorted_payload" << i << "=>" << "sorted_payload" << i << "_o" ;
					}
				}
				cout << param.str() <<endl;

				OperatorPtr op = newInstance("SortingNetwork", "sort_network", param.str(), inmap.str(), outmap.str());
				SortingNetwork* op_sort = dynamic_cast<SortingNetwork*>(op);
				sort_used = op_sort->flat_sort;
			
			}	
		} else { // Tao Sort
			inmap << "key0=>key0_i";
			outmap << "sorted_index0=>sorted_index0";
			for (int i =1; i < N; i++) {
				inmap << ",key" << i << "=>" << "key" << i << "_i";
				outmap << ",sorted_index" << i << "=>" << "sorted_index" << i;
			}
			cout << param.str() <<endl;

			newInstance("TaoSort", "sort_tao", param.str(), inmap.str(), outmap.str());
			
			//mux payload and key
			for (int i =0; i < N; i++) {
				vhdl << "sorted_key" << i << "_o <="; 
				for (int j = 0; j < N; j++) {
					if (direction) { // increasing
						vhdl << tab << "key" << j << "_i when sorted_index" << j << "= \"" << unsignedBinary(N-1-i, wIndex) << "\" else " << endl;
					} else { //decreasing
						vhdl << tab << "key" << j << "_i when sorted_index" << j << "= \"" << unsignedBinary(i, wIndex) << "\" else " << endl;
					}
				}
				vhdl << tab << "\"" << string(wKey, '-') << "\";" << endl;
			}
		 	if (wPayload != 0) {	
				for (int i =0; i < N; i++) {
					vhdl << "sorted_payload" << i << "_o <="; 
					for (int j = 0; j < N; j++) {
						if (direction) { // increasing
							vhdl << tab << "payload" << j << "_i when sorted_index" << j << "= \"" << unsignedBinary(N-1-i, wIndex) << "\" else " << endl;
						} else { //decreasing
							vhdl << tab << "payload" << j << "_i when sorted_index" << j << "= \"" << unsignedBinary(i, wIndex) << "\" else " << endl;
						}
					}
					vhdl << tab << "\"" << string(wPayload, '-') << "\";" << endl;
				}
			}
		}

		addFullComment("End of vhdl generation");
	};

	void SortWrapper::emulate(TestCase * tc) {
		// If it was a sorting network, check like sorting network (TODO this is duplicate code from SortingNetworks, maybe this can be done better)
		if (method == 0) {
			//THROWERROR("Testbench is not implemented for SortWrapper when using sorting networks. If you used indexPayload=false, please try using the SortingNetwork operator directly. Otherwise, we're sorry.");
			
			// the bitonic sort is not stable, so have to use the bitonic sort method to get the right result
			// and then verify that the order is correct, the VHDL will be checked against this.
			pair<mpz_class, mpz_class> list_to_sort[N];
			pair<mpz_class, mpz_class> val;
			for (int i =0; i < N; i++) {
				string is = to_string(i);
				if (wPayload != 0) {
					val = std::make_pair(tc->getInputValue("key" + is + "_i"), tc->getInputValue("payload" + is + "_i"));
				} else {
					val = std::make_pair(tc->getInputValue("key" + is + "_i"), 0);
				}
				list_to_sort[i] = val;
			}

			// Reuse the sort from BitonicSort
			for (auto swap : sort_used) { // swap is a pair, if first > second then swap
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
						cout << j << ", " << (list_to_sort[j].first) << endl;
					}
					THROWERROR("Sort does not work for those entries");
					break;
				}
			}

			// Add expected output to check VHDL against correct sort
			if (direction) {
				for (int i =0; i < N; i++) {
					string is = to_string(i);
					tc->addExpectedOutput ("sorted_key" + is + "_o", list_to_sort[i].first);
					if (wPayload != 0) {
						tc->addExpectedOutput ("sorted_payload" + is + "_o", list_to_sort[i].second);
					}
				}
			} else {
				for (int i =0; i < N; i++) {
					string is = to_string(N-1-i);
					tc->addExpectedOutput ("sorted_key" + is + "_o", list_to_sort[i].first);
					if (wPayload != 0) {
						tc->addExpectedOutput ("sorted_payload" + is + "_o", list_to_sort[i].second);
					}
				}
			}
			
		} else {
			// In this case use Tao's sort. This only outputs the indices, we need to add the keys and payload in order

			// We use the Tao sort method to get the right result
			// and then verify that the order is correct, the VHDL will be checked against this.
			pair<mpz_class, mpz_class> list_to_sort[N];
			pair<mpz_class, mpz_class> sorted_list[N];
			pair<mpz_class, mpz_class> val;
			int list_of_indices[N];
			for (int i =0; i < N; i++) {
				string is = to_string(i);
				if (wPayload != 0) {
					val = std::make_pair(tc->getInputValue("key" + is + "_i"), tc->getInputValue("payload" + is + "_i"));
				} else {
					val = std::make_pair(tc->getInputValue("key" + is + "_i"), 0);
				}
				list_to_sort[i] = val;
			}
			
			// Reuse the Tao sorting method
			int c_matrix[N][N];
			for (int i=0; i < N; i++) {
				for (int j=0; j < N; j++) {
					c_matrix[i][j] = (list_to_sort[i].first >= list_to_sort[j].first) ? 0 : 1;
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
				if (sorted_list[i].first < sorted_list[i+1].first) {
					for (int j = 0; j < N; j++) {
						cout << j << ", " << sorted_list[j].first << endl;
					}
					THROWERROR("Sort does not work for those entries");
					break;
				}
			}

			// Add expected output to check VHDL against correct sort
			if (direction) {
				for (int i =0; i < N; i++) {
					string is = to_string(N-1-i);
					tc->addExpectedOutput ("sorted_key" + is + "_o", sorted_list[i].first);
					if (wPayload != 0) {
						tc->addExpectedOutput ("sorted_payload" + is + "_o", sorted_list[i].second);
					}
				}
			} else {
				for (int i =0; i < N; i++) {
					string is = to_string(i);
					tc->addExpectedOutput ("sorted_key" + is + "_o", sorted_list[i].first);
					if (wPayload != 0) {
						tc->addExpectedOutput ("sorted_payload" + is + "_o", sorted_list[i].second);
					}
				}
			}
		}

	}


	void SortWrapper::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}




	OperatorPtr SortWrapper::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int N, wKey, wPayload, method;
		bool direction, indexPayload;
		ui.parseInt(args, "N", &N);
		ui.parseInt(args, "wKey", &wKey); 
		ui.parseInt(args, "wPayload", &wPayload); 
		ui.parseBoolean(args, "direction", &direction); 
		ui.parseBoolean(args, "indexPayload", &indexPayload); 
		ui.parseInt(args, "method", &method); 
		return new SortWrapper(parentOp, target, N, wKey, wPayload, direction, indexPayload, method);
	}
}//namespace
