/*
  OptimalDepthSort object for FloPoCo

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

#include "flopoco/SortingNetworks/OptimalDepthSort.hpp"
#include "flopoco/utils.hpp"
#include <sstream>

namespace flopoco{


	OptimalDepthSort::OptimalDepthSort(int N_):
		N(N_)
	{
		// Bitonic sort
		if (N==9) {
			flat_sort = flat_sort_9;
		} else if (N==17) {
			flat_sort = flat_sort_17;
		} else {
			flat_sort = {};
			std::cerr << "Optimal depth sort not implemented for N=" << std::to_string(N) << ", bitonic sort used instead\n";
		}
	}
	
	OptimalDepthSort::~OptimalDepthSort(){}

} //namespace

