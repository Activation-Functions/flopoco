#ifndef FIXRCF_HPP
#define FIXRCF_HPP

#include "Operator.hpp"
#include "utils.hpp"

#include <iostream>
#include <sstream>
#include <vector>

#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>
#include <string>

#include "sollya.h"

#include "Operator.hpp"

#include "BitHeap.hpp"
#include "ConstMult/FixRealKCM.hpp"

#include "FixFIR.hpp"

namespace flopoco{

	class FixRCF : public Operator
	{
	public:

		FixRCF(Target* target, int p_, int N_, double alpha_ = 1, bool signedInput_ = false, map<string, double> inputDelays = emptyDelayMap);

		/*
		FixRRC(Operator* parentOp, Target* target, int p_, int N_, double aplha_ = 1,
							 BitHeap* bitheap,
							 bool signedInput_ = false, map<string, double> inputDelays = emptyDelayMap);
		*/
		virtual ~FixRCF();

		// Overloading the virtual functions of Operator
		/**
		 * @brief the emulate function is used to simulate in software the operator
		 * in order to compare this result with those outputed by the vhdl opertator
		 */
		void emulate(TestCase* tc);

		/** @brief function used to create Standard testCase defined by the developper */
		void buildStandardTestCases(TestCaseList* tcl);

		/// \todo: revert back to returning an mpfr_t
		long double getFixRCFilterCoeff(int i);
		string getFixRCFilterCoeffString(int i);


		int p;  					/**< The precision of inputs and outputs */
		int N;  					/**< The number of taps */
		double alpha;					/**< The roll-of factor */

		bool signedInput;			/**< The inputs are signed, or not */

		mpfr_t mpcoeff[10000];  	/**< The coefficients of the RRC filter, as MPFR numbers */
		vector<string> coeff;		/**< the coefficients as strings */

		int wO;  					/**< output size, will be computed out of the constants */
		int g;						/**< The number of guard bits used for the computations */
	};

}


#endif
