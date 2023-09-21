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

#ifndef BITONICSORT_HPP
#define BITONICSORT_HPP
#include <vector>
#include <gmpxx.h>
#include "flopoco/utils.hpp"

namespace flopoco{

	class BitonicSort
	{
	public:
		/* The Bitonic sort constructor 
		 * @param[in] N is the number of objects to sort in the network
		 */
		typedef std::vector<std::pair<int, int>> sortStage;
		typedef std::vector<std::vector<std::pair<int, int>>> Sort;

		BitonicSort(int N_);

		sortStage BitonicSortREC(int N, bool direction, int k);

		sortStage BitonicMergeREC(int N, bool direction, int k);

		~BitonicSort();

		/** Number of objects to sort */
		int N;
		
	        Sort bitonicSort;	
	        sortStage flat_sort;	
	};

} //namespace
#endif
