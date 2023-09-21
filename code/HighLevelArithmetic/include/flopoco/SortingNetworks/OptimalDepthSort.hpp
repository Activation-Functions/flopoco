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

#ifndef OPTIMALDEPTHSORT_HPP
#define OPTIMALDEPTHSORT_HPP
#include <vector>
#include <gmpxx.h>
#include "flopoco/utils.hpp"

namespace flopoco{

	class OptimalDepthSort
	{
	public:
		/* The Bitonic sort constructor 
		 * @param[in] N is the number of objects to sort in the network
		 */
		typedef std::vector<std::pair<int, int>> sortStage;
		typedef std::vector<std::vector<std::pair<int, int>>> Sort;

		OptimalDepthSort(int N_);

		~OptimalDepthSort();

		/** Number of objects to sort */
		int N;
		
		// When adding a new sort, please also add in the cpp file in the if

	        sortStage flat_sort;
		// PARBERRY, Ian. A computer-assisted optimal depth lower bound for nine-input sorting networks. Mathematical systems theory, 1991, vol. 24, no 1, p. 101-116.	
	        sortStage flat_sort_9 =	{ {0, 7}, {1, 6}, {2, 5}, {3, 4}, // first layer : compare and swap 0,7, etc
	      				{0, 3}, {1, 2}, {4, 7}, {6, 8}, // second layer : compare and swap 0,3, etc
					{0, 1}, {2, 6}, {3, 4}, {5, 8},
					{1, 2}, {3, 5}, {4, 6}, {7, 8},
					{1, 3}, {2, 4}, {5, 7}, 
					{0, 1}, {2, 3}, {4, 5}, {6, 7},
					{3, 4}, {5, 6}};
		//EHLERS, Thorsten et MÜLLER, Mike. New bounds on optimal sorting networks. In : Evolving Computability: 11th Conference on Computability in Europe, CiE 2015, Bucharest, Romania, June 29-July 3, 2015. Proceedings 11. Springer International Publishing, 2015. p. 167-176.
	        sortStage flat_sort_17 = { {1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}, {13, 14}, {15, 16},
					{1, 3}, {2, 4}, {5, 7}, {6, 8}, {9, 11}, {10, 12}, {13, 15}, {14, 16}, 
					{1, 5}, {2, 6}, {3, 7}, {4, 8}, {9, 13}, {10, 14}, {11, 15}, {12, 16},
					{0, 3}, {1,13}, {2, 10}, {4, 7}, {5, 11}, {6, 12}, {8, 16}, {14, 15}, 
					{0, 13}, {1, 16}, {2, 5}, {3, 6}, {4, 14}, {7, 15}, {8, 9}, {10, 11},
					{0, 1}, {2, 8}, {3, 4}, {5, 10}, {6, 13}, {7, 11}, {9, 15}, {12, 14},
					{1, 5}, {2, 15}, {3, 8}, {4, 10}, {6, 7}, {9, 12}, {11, 13},
					{0, 2}, {1, 3}, {4, 6}, {5, 8}, {7, 9}, {10, 11}, {12, 14}, {13, 15},
					{0, 1}, {2, 3}, {4, 5}, {6, 8}, {7, 10}, {9, 11}, {12, 13}, {14, 15},
					{1, 2}, {3, 4}, {5, 6}, {7, 8}, {9, 10}, {11, 12}, {13, 14}, {15, 16}};
	};

} //namespace
#endif
