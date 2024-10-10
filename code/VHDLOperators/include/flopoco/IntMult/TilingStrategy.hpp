#pragma once

#include "BaseMultiplierCollection.hpp"
#include "BaseMultiplierCategory.hpp"
#include <gmpxx.h>
#include "flopoco/report.hpp"

namespace flopoco {

/*!
 * The TilingStragegy class
 */
	class TilingStrategy {

	public:
		typedef pair<int, int> multiplier_coordinates_t;
		typedef pair<BaseMultiplierCategory::Parametrization, multiplier_coordinates_t> mult_tile_t;
		TilingStrategy(int wX, int wY, bool signedIO, mpz_class errorBound, BaseMultiplierCollection* baseMultiplierCollection);

		virtual ~TilingStrategy() {}

		virtual void solve() = 0;
		void printSolution();
		void printSolutionTeX(ofstream &outstream, int wOut, bool triangularStyle=false);
		void printSolutionSVG(ofstream &outstream, int wOut, bool triangularStyle=false);

		int getwX();
		int getwY();
		list<mult_tile_t>& getSolution();
		mpz_class  getErrorCorrectionConstant();
		unsigned int getActualLSB();
		
	protected:
		/*!
		 * The solution data structure represents a tiling solution
		 *
		 * solution.first: type of multiplier, index used in BaseMultiplierCollection
		 * solution.second.first: x-coordinate
		 * solution.second.second: y-coordinate
		 *
		 */
		list<mult_tile_t> solution;

		int wX;                         /**< the width for X after possible swap such that wX>wY */
		int wY;                         /**< the width for Y after possible swap such that wX>wY */
		bool signedIO;                   /**< true if the IOs are two's complement */
		mpz_class errorBound;  /** the error bound allowed for this tiling, expressed in ulps of the exact result */
		unsigned int actualLSB;  /** this tiling will add bits only on columns >= actualLSB. 0 for an exact multiplier, >0 for a truncated one. This replaces guardBits, which can be deduced from it */
		mpz_class errorCorrectionConstant;       /** the constant to add to the bit heap to recenter the truncation error, expressed in ulps of the exact result. Should be at most errorBound */
		mpz_class error;       /** the error actually achieved by the tiling, , expressed in ulps of the exact result. Should be at most errorBound */
		BaseMultiplierCollection* baseMultiplierCollection;
		Target* target = NULL;

	};
}
