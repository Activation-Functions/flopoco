/*
  Bitonic object for FloPoCo

  Authors: Oregane Desrentes

  This file is part of the FloPoCo project
  developed by the Aric team at Ecole Normale Superieure de Lyon
	then by the Socrate then Emeraude team at INSA de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

*/
#include "gmp.h"
#include "mpfr.h"

#include "flopoco/SortingNetworks/BitonicSort.hpp"
#include <sstream>

namespace flopoco{


	BitonicSort::BitonicSort(int N_):
		N(N_)
	{
		// Bitonic sort
		flat_sort = BitonicSortREC(N, false, 0);

		// For a graphic representation of the sorting network, it is necessary to separate
		// the different stages of the sort, where a stage is defined as all the comparisons can be done in parallel.
		// However, because of how VHDL code works, it is not necessary to do this in the VHDL code, and just
		// the flat representation of the sort is enough, as all the parallel comparisons are done in parallel automatically

		// This was still scheduled with as-late-as-possible scheduling just in case (but useless at this time)
		// We are constructing a graph where each vertex is a comparison in the network
		// and u -> v if u uses a wire v used before (so the other way around)
		// This graph is represented with an adjacency list.
#if 0		
		// make graph
		int size = flat_sort.size();
		std::vector<int> adj[size];
		for (int u=0; u < size; u++) {
		      for (int v=u+1; v < size; v++) {
				if (flat_sort[u].first == flat_sort[v].first ||
				    flat_sort[u].first == flat_sort[v].second ||
				    flat_sort[u].second == flat_sort[v].first ||
				    flat_sort[u].second == flat_sort[v].second) {
					adj[v].push_back(u);
				}
		      }
		}


		bool empty_list = true;
		while (empty_list) {
			// Find vertices to add to the stage
			BitonicSort::sortStage current_stage;
			std::vector<int> added_vertex;
			for (int u = 0; u < size; u++) {
				if (adj[u].empty()) {
					current_stage.push_back(flat_sort[u]);
					added_vertex.push_back(u);
					adj[u].push_back(-1);
				}
			}
			// remove the extra vertices from the adjacency list and check the while condition
			empty_list = false;
			for (int u = 0; u < size; u++) {
				for (int v : added_vertex) {
					std::remove(adj[u].begin(), adj[u].end(), v);
				}
				if (adj[u].empty()) {
					empty_list = true;
				}
			}
			bitonicSort.push_back(current_stage);
		}
#endif
	}

	BitonicSort::sortStage BitonicSort::BitonicSortREC(int N, bool direction, int k) {
		BitonicSort::sortStage returnVector;
		std::pair<int, int> comp;
		if (N==1) {
		// return empty vector
		} else if (N==2) {
			if (direction) {
				comp = std::make_pair(k+1, k);
				returnVector.push_back(comp);
			} else {
				comp = std::make_pair(k, k+1);
				returnVector.push_back(comp);
			}
		} else {
			int top_half, bottom_half;
			BitonicSort::sortStage sort_top, sort_bottom, merge_sort;
			
			top_half = N/2;
			bottom_half = N - top_half;
			sort_top = BitonicSortREC(top_half, not direction, k);
			sort_bottom = BitonicSortREC(bottom_half, direction, k+top_half);
			merge_sort = BitonicMergeREC(N, direction, k);

			returnVector = sort_top;
			returnVector.insert(returnVector.end(), sort_bottom.begin(), sort_bottom.end());
			returnVector.insert(returnVector.end(), merge_sort.begin(), merge_sort.end());
		}
		return returnVector;

	}

	BitonicSort::sortStage BitonicSort::BitonicMergeREC(int N, bool direction, int k) {

		int top_half, bottom_half, power_of_2, power_of_2_N_div_2;
		
		top_half = N/2;
		bottom_half = N - top_half;
		power_of_2 = intlog2(N-1);
		power_of_2_N_div_2 = 1 << (power_of_2-1);

		BitonicSort::sortStage returnVector;
		std::pair<int, int> comp;

		if (N==1) {
			// empty vector
		} else if (N==2) {
			if (direction) {
				comp = std::make_pair(k+1, k);
				returnVector.push_back(comp);
			} else {
				comp = std::make_pair(k, k+1);
				returnVector.push_back(comp);
			}
		} else {
			for (int i = 0; i < power_of_2_N_div_2; i++) {
				if (power_of_2_N_div_2 + i < N) {
					if (direction) {
						comp = std::make_pair(k+power_of_2_N_div_2 + i, k+i);
					} else {
						comp = std::make_pair(k+i, k+power_of_2_N_div_2 + i);
					}
					returnVector.push_back(comp);
				}
			}
			BitonicSort::sortStage merge_top, merge_bottom;
			merge_top = BitonicMergeREC(power_of_2_N_div_2, direction, k);
			merge_bottom = BitonicMergeREC(N - power_of_2_N_div_2, direction, k+ power_of_2_N_div_2);

			returnVector.insert(returnVector.end(), merge_top.begin(), merge_top.end());
			returnVector.insert(returnVector.end(), merge_bottom.begin(), merge_bottom.end());
		}
		return returnVector;
	}

	BitonicSort::~BitonicSort()
	{
		// est-ce qu'il faut que je supprime le vecteur bitonicSort ici ? j'y comprends rien au c++
	}

} //namespace

