#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "Fix2DNormCORDIC.hpp"

//#include "FixFunctions/FixFunctionByTable.hpp"
// #include "FixFunctions/BipartiteTable.hpp"
// #include "FixFunctions/FixFunctionByPiecewisePoly.hpp"


using namespace std;
namespace flopoco {

	Fix2DNormCORDIC::Fix2DNormCORDIC(OperatorPtr parentOp, Target* target_, int lsbIn_, int lsbOut_) :
		Fix2DNorm(parentOp, target_, lsbIn_, lsbOut_)
	{		
		srcFileName = "Fix2DNormCORDIC";
		setCopyrightString("Romain Bouarah, Florent de Dinechin (2022-...)");
		
		ostringstream name;
		int wIn = getWIn();
		int wOut = getWOut();				
		
		name << "Fix2DNormCORDIC_" << wIn << "_" << wOut << "_uid" << getNewUId();
		setNameWithFreqAndUID (name.str());

		computeGuardBits ();
		initKFactor();
		
		buildCordic ();
		buildKDivider ();

		vhdl << tab << "R <= RR" << range(-lsbOut, 0) << ";" << endl;
	}

	/* Input  : X, Y
	 * Output : RK */
	void Fix2DNormCORDIC::buildCordic () {
		int wOut = getWOut();
		int wIn = getWIn();
		// TODO: Replace wIn by max(wIn, wOut) ?
		int sizeX = 2 + wIn + guard;
		int sizeY = sizeX;

		/* CORDIC initialisation */
		/* X_0 */
		/* Turning ufix(-1, lsbIn) X into ufix(1, lsbIn - guard) */
		vhdl << tab << declare("X0", sizeX) << " <= \"00\" & X & " << zg(sizeX - 2 - wIn) << ";" << endl;
		/* Y_0 */
		vhdl << tab << declare("Y0", sizeY) << " <= \"00\" & Y & " << zg(sizeY - 2 - wIn) << ";" << endl;

		
		/* First iteration */
		vhdl << tab << "--- Iteration 1 : sign is known positive ---" << endl;
		/* X_1 */
		vhdl << tab << declare(getTarget()->adderDelay(sizeX), "X1", sizeX) << " <= X0 + Y0;" << endl;
		/* Y_1 */
		vhdl << tab << declare(getTarget()->adderDelay(sizeY), "Y1", sizeY) << " <= Y0 - X0;"  << endl;				

		
	        int stage;
		for (stage = 1; stage <= maxIterations; stage++) {
			REPORT(DEBUG, "stage=" << stage);
			vhdl << tab << "--- Iteration " << stage + 1 << " ---" << endl;

			/* sign(Y_i) */
			vhdl << tab << declare(join("sgnY", stage)) << " <= " << join("Y", stage) << of(sizeY - 1) << ";" << endl;

			/* X_{i+1} */
			vhdl << tab << declare(join("YShift", stage), sizeX) << " <= "
				    << rangeAssign(sizeX - 1, sizeX - stage, join("sgnY", stage))   /* sign extension */
				    << " & Y" << stage << range(sizeY - 1, stage) << ";" << endl;

			vhdl << tab << declare(getTarget()->fanoutDelay(sizeX+1) + getTarget()->adderDelay(sizeX), join("X", stage+1), sizeX) << " <= "
				    << join("X", stage) << " - " << join("YShift", stage) << " when " << join("sgnY", stage) << "=\'1\'     else "
				    << join("X", stage) << " + " << join("YShift", stage) << ";" << endl;

			/* Y_{i+1} */
			vhdl << tab << declare(join("XShift", stage), sizeY) << " <= " << zg(stage) << " & X" << stage << range(sizeX - 1, stage) << ";" <<endl;			
			vhdl << tab << declare(join("Y", stage + 1), sizeY) << " <= "
				    << join("Y", stage) << " + " << join("XShift", stage) << " when " << join("sgnY", stage) << "=\'1\'     else "
				    << join("Y", stage) << " - " << join("XShift", stage) << ";" << endl;			
		}

		/* RK is an ufix(1, lsbIn - guard) */
		vhdl << tab << declare("RK", sizeX) << " <= X" << stage << ";" << endl;
	}


	
	static inline int bits2digits (mpfr_prec_t b) {
		const double log10_2 = log10(2);
		return (int)(floor (b*log10_2));
	}

	/* Input  : ufix(1, lsbIn - guard) RK
	 * Output : ufix(1, lsbOut - 1) RR */	 
	void Fix2DNormCORDIC::buildKDivider() {
		mpfr_prec_t kfactor_prec = mpfr_get_prec (kfactor);
		int digits = bits2digits (kfactor_prec);
	        char *buffer = (char*)(malloc (digits + 3)); // "0." + #digits + '\0'
		string format = "%." + to_string(digits) + "RNf";
		  
	        mpfr_sprintf (buffer, format.c_str(), kfactor);
		
		string kfactor_str = string(buffer);
		
		string args = "method=KCM"				\
			      " signedIn=0"				\
			      " msbIn=1"				\
			      " lsbIn=" + to_string(lsbIn - guard) +
			      " lsbOut=" + to_string(lsbOut) +
			      " constant=" + kfactor_str +
			      " targetUlpError=0.75";
			      

		newInstance("FixRealConstMult", "kfactorDivider",
					        args,
						"X=>RK",
						"R=>RR");

		free (buffer);
	}
  
	void Fix2DNormCORDIC::initKFactor () {
		int wOut = getWOut();
		mpfr_t temp;
		
		mpfr_init2 (kfactor, 10*(wOut + getWIn()));
		mpfr_init2 (temp, 10*(wOut + getWIn()));

		mpfr_set_ui (kfactor, 1, MPFR_RNDN);
		for (int i = 0; i <= maxIterations; i++) {
			mpfr_set_ui (temp, 1, MPFR_RNDN);          // temp = 1
			mpfr_div_2ui (temp, temp, 2*i, MPFR_RNDN); // temp = temp/2^(2i) = 2^(-2i)
			mpfr_add_ui (temp, temp, 1, MPFR_RNDN);    // temp = (1 + 2^(-2i))
			
			mpfr_mul (kfactor, kfactor, temp, MPFR_RNDN);
		}
		mpfr_sqrt (kfactor, kfactor, MPFR_RNDN);
		mpfr_ui_div(kfactor, 1, kfactor, GMP_RNDN);
		
		REPORT (DEBUG, "kfactor=" << printMPFR (kfactor));
		mpfr_clear (temp);
	}

	void Fix2DNormCORDIC::computeGuardBits () {
		maxIterations = ceil(1 + (3 - lsbOut)/2);
		
		double delta = 0.5; // error in ulp
		double shift = 0.5;
		for (int i=1; i <= maxIterations; i++) {
			delta = delta*(1 + shift) + 1;
			shift *= 0.5;
		}
		
		guard = ceil(log2(delta) + lsbIn - lsbOut + 3); // - log2(kfactor)

		REPORT(DETAILED, "Number of iterations=" << maxIterations);
		REPORT(DETAILED, "Guard bits=" << guard);
	}
  
	Fix2DNormCORDIC::~Fix2DNormCORDIC () {
		mpfr_clear (kfactor);
	}
}

