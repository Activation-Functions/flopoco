#ifndef _VARYINGPIECEWISEPOLYAPPROX_HPP_
#define _VARYINGPIECEWISEPOLYAPPROX_HPP_

#include <iostream>
#include <string>

#include <gmpxx.h>
#include <sollya.h>

#include "flopoco/FixConstant.hpp"
#include "flopoco/FixFunctions/BasicPolyApprox.hpp"
#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp" // mostly for reporting

using namespace std;

/* Stylistic convention here: all the sollya_obj_t have names that end with a capital S */
namespace flopoco{

	/**
	 * The PiecewisePolyApprox object builds and maintains a piecewise machine-efficient
	 * polynomial approximation to a fixed-point function over [0,1]
	*/

	class VaryingPiecewisePolyApprox  {
	public:

		/**
		 * A minimal constructor
		 */
	  VaryingPiecewisePolyApprox(FixFunction* f, double targetAccuracy, int lsbIn, int lsbOut);

		/**
		 * A minimal constructor that parses a sollya string
		 */
	  VaryingPiecewisePolyApprox(string sollyaString, double targetAccuracy, int lsbIn, int lsbOut);

		virtual ~VaryingPiecewisePolyApprox();

		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		/**
		 * get the bits of coeff of degree d of polynomial number i
		 */
		mpz_class getCoeff(int i, int d);

		/**
		 * function that regroups most of the constructor code
		 */
		void build();

	        bool tabulateRest; 
	        int nbInterval;                   /**< the total number of intervals the domain is split into */
		int degree;                        /**< degree of the polynomial approximations */
		vector<BasicPolyApprox*> poly;     /**< The vector of polynomials, eventually should all be on the same format */
		int LSB;                           /**< common weight of the LSBs of the polynomial approximations */
		vector<int> MSB;                   /**< vector of MSB weights for each coefficient */
		double approxErrorBound;           /**< guaranteed upper bound on the approx error of each approximation provided. Should be smaller than targetAccuracy */
		vector<int> coeffSigns;            /**< If all the coeffs of a given degree i are strictly positive (resp. strictly negative), then coeffSigns[i]=+1 (resp. -1). Otherwise 0 */
	  vector<mpz_class> table;
	private:

		/**
		 * a local function to build g_i(x) = f(2^(-alpha-1)*x + i*2^-alpha + 2^(-alpha-1)) defined on [-1,1]
		 */
		sollya_obj_t buildSubIntervalFunction(sollya_obj_t fS, int i);
	        sollya_obj_t buildFinalSubIntervalFunction(sollya_obj_t fS, int i);

		/**
		 * a local function to open the cache file, and create it if necessary
		 * @param cacheFileName the name of the cache file
		 */
		void openCacheFile(string cacheFileName);

		/**
		 * a local function to write the polynomials' parameters to the cache file
		 * @param cacheFileName the name of the cache file
		 */
		void writeToCacheFile(string cacheFileName);

		/**
		 * a local function to read the polynomials' parameters from the cache file
		 * @param cacheFileName the number of intervals the domain is split into
		 * @nbIntervals number of intervals, used in build() function
		 */
		void readFromCacheFile(string cacheFileName);

		/**
		 * check whether all the coefficients of a given degree are of the same sign
		 */
		void checkCoefficientsSign();

		/**
		 * a local function to report on the parameters of the polynomials
		 * @param nbIntervals the number of intervals the domain is split into
		 */
		void createPolynomialsReport();

	        int lsbIn;
	        int msbOut;
	        int lsbOut;
		FixFunction *f;                    /**< The function to be approximated */
		double targetAccuracy;             /**< please build an approximation at least as accurate as that */

		string srcFileName;                /**< useful only to enable same kind of reporting as for FloPoCo operators. */
		string uniqueName_;                /**< useful only to enable same kind of reporting as for FloPoCo operators. */
		bool needToFreeF;                  /**< in an ideal world, this should not be needed */

		fstream cacheFile;                 /**< file storing the cached parameters for the polynomials */
	       
	};

}
#endif // _POLYAPPROX_HH_


// Garbage below
