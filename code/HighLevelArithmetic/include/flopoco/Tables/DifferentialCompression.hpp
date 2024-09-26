#pragma once
#include <vector>
//#include <set>
//#include <map>
//#include <memory>
#include <iostream>
//#include <fstream>
#include <string>
//#include <sstream>
#include <gmpxx.h>
//#include <float.h>
//#include <utility>

#include "flopoco/report.hpp"
#include "flopoco/utils.hpp"

using namespace std;

#include "flopoco/Target.hpp"
#include "flopoco/Tables/TableCostModel.hpp"

namespace flopoco {
	class DifferentialCompression {
	public:
		vector<mpz_class> subsampling;
		vector<mpz_class> diffs;
		int subsamplingIndexSize;
		int subsamplingWordSize;
		int diffWordSize;
		int diffIndexSize; // also originalWin
		int originalWout;
		mpz_class originalCost;
		mpz_class subsamplingCost;
		mpz_class diffCost;

		/**
		 * Find an errorless compression of a table as a sum of a subsampling + offset
		 * @param[in] values       The values of the table to compress
		 * @param[in] wIn          The initial index width of the table to compress
		 * @param[in] wOut         The output word size of the table to compress
		 * @param[in] costModel    The cost function which will be optimised by the method
		 * @param[in] target       The target for which the optimisation is performed
		 * @return                 A DifferentialCompression containing the values and the parameters of the compression for this table
		 */
		static DifferentialCompression find_differential_compression(vector<mpz_class> const & values, int wIn, int wOut, Target * target, table_cost_function_t cost);

		/**
		 * Call find_differential_compression with default cost model
		 * @param[in] values       The values of the table to compress
		 * @param[in] wIn          The initial index width of the table to compress
		 * @param[in] wOut         The output word size of the table to compress
		 * @param[in] target       The target for which the optimisation is performed
		 * @return                 A DifferentialCompression containing the values and the parameters of the compression for this table
		 */
		static DifferentialCompression find_differential_compression(vector<mpz_class> const & values, int wIn, int wOut, Target * target);


		/**
		 * @brief Uncompress the table
		 * @return a vector of mpz_class corresponding to the stored values
		 */
		vector<mpz_class> getInitialTable() const;

		/**
		 * @brief Compute the size in bits of the subsampling table content
		 * @return the size as a size_t
		 */
		size_t subsamplingStorageSize() const;

		/**
		 * @brief Compute the size in bits of the diffs table content
		 * @return the size as a size_t
		 */
		size_t diffsStorageSize() const;

		/**
		 * @brief Compute the left shift, in bits of the subsampling table content WRT the diff table content
		 */
		size_t subsamplingShift() const;


		string report() const;

	};
}