/*

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author:    Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

 */

#ifndef TABLE_HPP
#define TABLE_HPP
#include <vector>
#include <gmpxx.h>

/**
Representation of a table of values

*/

namespace flopoco
{

	class Table
	{
	public:
		/**
		 * The Table constructor
		 * @param[in] values 		the values used to fill the
		 table. Each value is a bit vector given as positive mpz_class.
		 * @param[in] wIn    		the width of the input in bits
		 (optional, may be deduced from values)
		 * @param[in] minIn			minimal input value, to
		 which value[0] will be mapped (default 0)
		 * @param[in] maxIn			maximal input value
		 (default: values.size()-1)
		 */
		Table(std::vector<mpz_class> _values, int _wIn = -1,
		      int _minIn = -1, int _maxIn = -1);

		
		Table();

		/**
		 * The Table initializer
		 * @param[in] values 		the values used to fill the
		 table. Each value is a bit vector given as positive mpz_class.
		 * @param[in] wIn    		the width of the input in bits
		 (optional, may be deduced from values)
		 * @param[in] minIn			minimal input value, to
		 which value[0] will be mapped (default 0)
		 * @param[in] maxIn			maximal input value
		 (default: values.size()-1)
		 */
		void initialize(std::vector<mpz_class> _values, int _wIn = -1,
		      int _minIn = -1, int _maxIn = -1);

		/** get one element of the table */
		mpz_class operator[](int x);

		bool full; /**< true if there is no "don't care" inputs, i.e.
			      minIn=0 and maxIn=2^wIn-1 */

		std::vector<mpz_class>
		    values; /**< the values used to fill the table */

		/** Input width (in bits)*/
		int wIn;

		/** Output width (in bits)*/
		int wOut;

		/** minimal input value (default 0) */
		mpz_class minIn;

		/** maximal input value (default 2^wIn-1) */
		mpz_class maxIn;

		private:
		bool initialised;
	};

} // namespace flopoco
#endif
