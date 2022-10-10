/*
  A generic class for tables of values

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

 */

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

#include "flopoco/Tables/Table.hpp"
#include "flopoco/utils.hpp"

using namespace std;
using std::begin;

namespace flopoco
{
	Table::Table(std::vector<mpz_class> _values, int _wIn, int _minIn, int _maxIn):
		initialised{false}
	{
		initialize(_values, _wIn, _minIn, _maxIn);
	}

	Table::Table():rox
		wIn{-1},minIn{-1},maxIn{-1},initialised{false}{}

	void Table::initialize(std::vector<mpz_class> _values, int _wIn, int _minIn, int _maxIn) {
		assert(!initialised && "Trying to initialise an already initialized Table");
		values = _values;
		wIn = _wIn;
		minIn = _minIn;
		maxIn = _maxIn;
		// sanity checks: can't fill the table if there are no values to
		// fill it with
		assert(values.size() != 0 &&
		       "Error in Table::init(): the set of values to be written in the table is empty");

		// set wIn
		if (wIn < 0) {
			// set the value of wIn
			wIn = intlog2(values.size());
		}

		assert(((unsigned)1 << wIn) >= values.size() &&
		       "Incoherent wIn / number of values");

		// determine the lowest and highest values stored in the table
		mpz_class maxValue = values[0], minValue = values[0];

		// this assumes that the values stored in the values array are
		// all positive
		for (auto const &value : values) {
			// TODO Handle the negative case : requires some
			// rewriting of the VHDL ops counter part
			assert(value >= 0 && "Tables cannot be initialized with negative values as of now");
			if (maxValue < value)
				maxValue = value;
			if (minValue > value)
				minValue = value;
		}

		// set wOut
		wOut = intlog2(maxValue);

		if (minIn < 0) {
			minIn = 0;
		}
		// set maxIn
		if (maxIn < 0) {
			maxIn = values.size() - 1;
		}

		assert(((1 << wIn) < maxIn) &&
		       "ERROR: in Table constructor: maxIn too large");

		// determine if this is a full table
		if ((minIn == 0) && (maxIn == (1 << wIn) - 1))
			full = true;
		initialised = true;
	}

	mpz_class Table::operator[](int x)
	{
		assert(initialised && "reading from uninitialised Table");
		assert((x >= minIn && x <= maxIn) &&
		       "Error in table: input index out of range");
		return values[x];
	}

} // namespace flopoco
