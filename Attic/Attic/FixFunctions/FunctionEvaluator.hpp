#ifndef FunctionEvaluator_HPP
#define FunctionEvaluator_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <mpfr.h>
#include <gmpxx.h>

#include "Operator.hpp"
#include "PolyCoeffTable.hpp"
#include "IntAdder.hpp"

namespace flopoco{


	/** @brief The FunctionEvaluator class */
	class FunctionEvaluator : public Operator
	{
	public:
		/**
		 * @brief The FunctionEvaluator constructor
		 */
		FunctionEvaluator(Target* target, string func, int wInX, int lsbOut, int n, bool finalRounding = true, map<string, double> inputDelays = emptyDelayMap);

		/**
		 * @brief FunctionEvaluator destructor
		 */
		~FunctionEvaluator();

		void emulate(TestCase * tc);

		PiecewiseFunction *pf;
		PolyCoeffTable *tg;
		PolynomialEvaluator *pe;

		int getRWidth();

		int getRWeight();

		protected:

		unsigned wR;
		int weightR;

		int wInX_;
		int lsbOut_;
		bool finalRounding_;

	};

}

#endif
