/*
  Integer constant multiplication using minimum number of ternary adders due to

  Kumm, M., Gustafsson, O., Garrido, M., & Zipf, P. (2018). Optimal Single Constant Multiplication using Ternary Adders.
  IEEE Transactions on Circuits and Systems II: Express Briefs, 65(7), 928–932. http://doi.org/10.1109/TCSII.2016.2631630

  Author : Martin Kumm kumm@uni-kassel.de, (emulate adapted from Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr)

  All rights reserved.

*/

#include <algorithm>
#include <iostream>
#include <sstream>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include "flopoco/ConstMult/IntConstMultShiftAdd.hpp"
#include "flopoco/ConstMult/IntConstMultShiftAddOptTernary.hpp"
#include "flopoco/ConstMult/tscm_solutions.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/UserInterface.hpp"
#include "flopoco/utils.hpp"

using namespace std;

#if defined(HAVE_PAGLIB) && defined(HAVE_OSCM)
#include "pagsuite/pagexponents.hpp"

#include "pagsuite/compute_successor_set.h"
#include "pagsuite/log2_64.h"
#include "pagsuite/fundamental.h"

using namespace PAGSuite;

namespace flopoco{

    IntConstMultShiftAddOptTernary::IntConstMultShiftAddOptTernary(Operator* parentOp, Target* target, int wIn, int coeff, bool isSigned, int epsilon) : IntConstMultShiftAdd(parentOp, target, wIn, "", isSigned, epsilon)
    {
		int maxCoefficient = 4194303; //=2^22-1

        int shift;
        int coeffOdd = fundamental(coeff, &shift);

		    if(coeffOdd <= maxCoefficient)
        {
            cout << "nofs[" << coeff << "]=" << nofs[(coeff-1)>>1][0] << " " << nofs[(coeff-1)>>1][1] << endl;

            int nof1 = nofs[(coeffOdd-1)>>1][0];
            int nof2 = nofs[(coeffOdd-1)>>1][1];

            set<int> coefficient_set;
            coefficient_set.insert(coeff);

            set<int> nof_set;
            if(nof1 != 0) nof_set.insert(nof1);
            if(nof2 != 0) nof_set.insert(nof2);

            string adderGraphString;

            computeAdderGraphTernary(nof_set, coefficient_set, adderGraphString);

            stringstream adderGraphStringStream;
            adderGraphStringStream << "{" << adderGraphString;

            if(adderGraphString.length() > 1) adderGraphStringStream << ",";
            adderGraphStringStream << "{'O',[" << coeff << "],1,[" << coeffOdd << "],1," << shift << "}";
            adderGraphStringStream << "}";

            cout << "adder_graph=" << adderGraphStringStream.str() << endl;

            ProcessIntConstMultShiftAdd(target,adderGraphStringStream.str());

            ostringstream name;
            name << "IntConstMultShiftAddOptTernary_" << coeffOdd << "_" << wIn;
            setName(name.str());

        }
        else
        {
			cerr << "Error: Coefficient too large, max. odd coefficient is " << maxCoefficient << endl;
            exit(-1);
        }

	}

    OperatorPtr flopoco::IntConstMultShiftAddOptTernary::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
        int wIn, constant, epsilon;
        bool isSigned;


        ui.parseInt( args, "wIn", &wIn );
        ui.parseInt( args, "constant", &constant );
        ui.parseBoolean(args, "signed", &isSigned);
		    ui.parseInt( args, "errorBudget", &epsilon );

        return new IntConstMultShiftAddOptTernary(parentOp, target, wIn, constant, isSigned, epsilon);
    }
}

TestList IntConstMultShiftAddOptTernary::unitTest(int testLevel)
{
  return IntConstMultShiftAddOpt::unitTest(testLevel);
}

namespace flopoco {
	template <>
	const OperatorDescription<IntConstMultShiftAddOptTernary> op_descriptor<IntConstMultShiftAddOptTernary> {
	    "IntConstMultShiftAddOptTernary", // name
	    "Integer constant multiplication using shift and ternary additions "
	    "in an optimal way (i.e., with minimum number of ternary adders). "
	    "Works for coefficients up to 4194303 (22 bit)", // description,
							     // string
	    "ConstMultDiv", // category, from the list defined in
			    // UserInterface.cpp
	    "",		    // seeAlso
	    "wIn(int): Input word size; \
                  constant(int): constant; \
                  signed(bool)=true: signedness of input and output; \
                  errorBudget(int)=0: Allowable error for truncated constant multipliers;",
	    "Nope."};
}

#endif