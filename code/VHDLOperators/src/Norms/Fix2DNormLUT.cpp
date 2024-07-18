#include <iostream>
#include <sstream>

#include "flopoco/Norms/Fix2DNormLUT.hpp"

using namespace std;
namespace flopoco {

	Fix2DNormLUT::Fix2DNormLUT(OperatorPtr parentOp, Target* target_, int lsbIn_, int lsbOut_) :
		Fix2DNorm(parentOp, target_, lsbIn_, lsbOut_)
	{
		srcFileName = "Fix2DNormLUT";
		setCopyrightString("Romain Bouarah, Florent de Dinechin (2022)");

		ostringstream name;
		int wIn = getWIn();
		int wOut = getWOut();

		name << "Fix2DNormLUT_" << wIn << "_" << wOut << "_uid" << getNewUId();
		setNameWithFreqAndUID (name.str());

		computeGuardBits();
		
		/* X^2 and Y^2 : ufix(-1, 2*lsbIn) */
		/* Computed manually with synthesis */
		string squareAdderXYArg;
		if (-5 <= lsbIn && lsbIn <= -7) {
			squareAdderXYArg = squarerByLUT();
		} else {		
			squareAdderXYArg = squarerByLUT();
			//			squareAdderXYArg = squarerByIntSquarer(); doesn't work without Gurobi
		}

		/* X^2 + Y^2 : ufix(0, 2*lsbIn) */
		int wInAdder = -2*lsbIn + 1;
		
		vhdl << tab << declare("square_adder_Cin") << " <= '0';" << endl;

		newInstance("IntAdder", "square_adder", "wIn=" + to_string(wInAdder),
							squareAdderXYArg + "Cin=>square_adder_Cin",
							"R=>S");
							
		/* Converting Sum into floating-point format s.t. :
		 *    S = 2^(-C_even) * SS
		 */
		newInstance("LZOC", "LZC",
				    "countType=0" \
				    " wIn=" + to_string(wInAdder),
				    "I=>S",
				    "O=>C");

		int wCount = sizeInBits(wInAdder);
		vhdl << tab << declare("C_even", wCount) << " <= C" << range(wCount - 1, 1) << " & '0';" << endl;
		
		newInstance("Shifter", "SShifter", 
				       "wX=" + to_string(wInAdder) +
				       " maxShift=" + to_string(wInAdder - 1) + /* TODO: odd and even case are different */
				       " wR=" + to_string(wInAdder) +
				       " dir=0",
				       "X=>S, S=>C_even",
				       "R=>SS"); // Shifted Sum

		vhdl << tab << declare("SS_trunc", wInAdder - guard) << " <= SS" << range(wInAdder - 1, guard) << ";" << endl;
		
		vhdl << tab << declare("C_sqrt", wCount - 1) << " <= C_even" << range(wCount - 1, 1) << ";" << endl;		
		
		/* sqrt */
		newInstance("FixFunctionByTable", "sqrt_LUT", "tableCompression=1" \
							      " f=sqrt(x*2)"       \
							      " signedIn=false"    \
							      " lsbIn=" + to_string(-wInAdder + guard) +
							      " lsbOut=" + to_string(lsbOut),
							      " X=>SS_trunc",
							      " Y=>RR");

		int maxShift = pow(2, wCount - 1) - 1;
		newInstance("Shifter", "RRShift",
				       "wX=" + to_string(1 - lsbOut) +
				       " maxShift=" + to_string(maxShift) +
				       " wR=" + to_string(1 - lsbOut) +
				       " dir=1",
				       "X=>RR, S=>C_sqrt",
				       "R=>R");
	}

	void Fix2DNormLUT::computeGuardBits() {
		guard = -2*lsbIn + lsbOut - 1;
		REPORT(LogLevel::DETAIL, "Guard bits=" << guard);
	}

	string Fix2DNormLUT::squarerByLUT() {
		string squarer_args = "tableCompression=1" \
				      " f=x^2"             \
				      " signedIn=false"    \
				      " lsbIn=" + to_string(lsbIn) +
				      " lsbOut=" + to_string(2*lsbIn);

	        newInstance("FixFunctionByTable", "squarerX", squarer_args,
							      " X=>X",
							      " Y=>XX");

		newInstance("FixFunctionByTable", "squarerY", squarer_args,
							      " X=>Y",
							      " Y=>YY");
		return "X=>XX, Y=>YY, ";
	}

	string Fix2DNormLUT::squarerByIntSquarer() {
		string squarer_args =
			"wIn=" + to_string(getWIn()) +
			" wOut=0"										 \
			" method=schoolbook"				 \
			" maxDSP=0"									 \
			" signedIn=false";
		newInstance("IntSquarer", "squarerX", squarer_args, "X=>X", "R=>XX");
		newInstance("IntSquarer", "squarerY", squarer_args, "X=>Y", "R=>YY");
		
		vhdl << tab << declare("XX_adder", -2*lsbIn + 1) << " <= '0' & XX;" << endl;
		vhdl << tab << declare("YY_adder", -2*lsbIn + 1) << " <= '0' & YY;" << endl;
		
		return "X=>XX_adder, Y=>YY_adder, ";
	}
}
