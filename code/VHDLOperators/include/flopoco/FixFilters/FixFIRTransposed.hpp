#ifndef FIXFIR_HPP
#define FIXFIR_HPP

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/FixFilters/FixSOPC.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco{


	class FixFIRTransposed : public Operator {

	public:
		
		/**
		 * @brief 				constructor, building the FIR out of the coefficients, with (a possibly different) lsbIn and lsbOut.
		 * @param coeff			The coefficients are considered as real numbers, provided as string expresssions such as 0.1564565756768 or sin(3*pi/8)
		 * Input is assumed to be in (-1,1) with lsb at postion lsbIn.
		 * @param lsbIn			Input has lsb at position lsbIn.
		 * @param lsbOut		Output has lsb at position lsbOut.
		 * @param symmetry	0 for normal filter, 1 if the coefficients are symmetric, -1 if they are antisymmetric. We could probably attempt to detect it 
		 * @param rescale
		 *						If rescale=false, the msb of the output is computed so as to avoid overflow.
		 *						If rescale=true, all the coefficients are rescaled by 1/sum(|coeffs|).
		 * This way the output is also in [-1,1], output size is equal to input size, and the output signal makes full use of the output range.
		*/
    FixFIRTransposed(OperatorPtr parentOp, Target* target, int wIn, int wOut, vector<int64_t> coeff, string adder_graph="", const string& graph_truncations="", const string& sa_truncations="", const int epsilon=0);

    void parseSAtruncations(const string& sa_truncations, vector<vector<int>>& truncations);

		void emulate(TestCase * tc);

    void buildStandardTestCases(TestCaseList * tcl);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

  private:
    int wIn;	/**< input word size of the filter */
    int wOut; /**< output word size of the filter */
    int noOfTaps;								/**< number of taps */
    vector<int64_t> coeffs;
    vector<mpz_class> xHistory; 			// history of x used by emulate
    int currentIndex;

    int epsilon; //max. allowable error for truncated constant multipliers (currently only used to check error for given truncations in testbench)
	};

}

#endif
