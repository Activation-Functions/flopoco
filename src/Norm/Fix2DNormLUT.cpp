#include <iostream>
#include <sstream>

#include "Fix2DNormLUT.hpp"

using namespace std;
namespace flopoco {

	Fix2DNormLUT::Fix2DNormLUT(OperatorPtr parentOp, Target* target_, int lsbIn_, int lsbOut_) :
		Fix2DNorm(parentOp, target_, lsbIn_, lsbOut_)
	{
		srcFileName = "Fix2DNormLUT";
		setCopyrightString("Romain Bouarah, Florent de Dinechin (2022-...)");

		ostringstream name;
		int wIn = getWIn();
		int wOut = getWOut();

		name << "Fix2DNormLUT_" << wIn << "_" << wOut << "_uid" << getNewUId();
		setNameWithFreqAndUID (name.str());

		/* X^2 & Y^2*/
		string squarer_args = "wIn=" + to_string(wIn) +
				      " wOut=0"            \
				      " method=schoolbook" \
				      " maxDSP=0"          \
				      " signedIn=false";
		newInstance("IntSquarer", "squarerX", squarer_args, "X=>X", "R=>XX");
		newInstance("IntSquarer", "squarerY", squarer_args, "X=>Y", "R=>YY");

		/* X^2 + Y^2 */
		int wInAdder = -2*lsbIn + 1;
		vhdl << tab << declare("XX_adder", wInAdder) << " <= '0' & XX;" << endl;
		vhdl << tab << declare("YY_adder", wInAdder) << " <= '0' & YY;" << endl;


		vhdl << tab << declare("square_adder_Cin") << " <= '0';"<<endl;
		newInstance("IntAdder", "square_adder", "wIn=" + to_string(wInAdder),
							"X=>XX_adder, Y=>YY_adder, Cin=>square_adder_Cin",
							"R=>S");

		/* sqrt */
		newInstance("FixFunctionByTable", "sqrt_LUT", "tableCompression=1" \
							      " f=sqrt(x*2)"       \
							      " signedIn=false"    \
							      " lsbIn=" + to_string(-wInAdder) +
							      " lsbOut=" + to_string(lsbOut),
							      " X=>S",
							      " Y=>RR");
		vhdl << tab << "R <= RR" << range(-lsbOut, 0) << ";" << endl;
	}
}
