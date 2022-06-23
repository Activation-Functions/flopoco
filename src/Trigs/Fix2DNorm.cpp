#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "Fix2DNorm.hpp"

//#include "FixFunctions/FixFunctionByTable.hpp"
// #include "FixFunctions/BipartiteTable.hpp"
// #include "FixFunctions/FixFunctionByPiecewisePoly.hpp"

#define _STRINGIFY(x) #x
#define STRINGIFY(x) _STRINGIFY(x)

#define KFACTOR_PRECISION 128
#define KFACTOR_FORMAT "%." STRINGIFY(KFACTOR_PRECISION) "RNf"

using namespace std;
namespace flopoco{



	Fix2DNorm::Fix2DNorm(OperatorPtr parentOp, Target* target_, int lsbIn_, int lsbOut_) :
		Operator(parentOp, target_), lsbIn(lsbIn_), lsbOut(lsbOut_)
	{
		initKFactor();	    
		
		srcFileName = "Fix2DNorm";
		setCopyrightString("Romain Bouarah, Florent de Dinechin (2022-...)");
		// useNumericStd_Unsigned();

		ostringstream name;
		// TODO: Error in name if lsb < 0 due to -
		name << "Fix2DNorm_"; // << msbOut << "_" << lsb;
		setNameWithFreqAndUID(name.str());

		// TODO: Refactor
		int wIn = getWIn();
		int wOut = getWOut();
		int maxIterations = wOut - 1;
		
		addInput  ("X", wIn);
		addInput  ("Y", wIn);
		addOutput ("R", wOut, 2);

		computeGuardBits ();
		
		buildCordic (maxIterations);	       		
		
		// TODO: Compute needed precision from lsbIn and lsbOut
	        char buffer[KFACTOR_PRECISION + 3]; // len("1.") + KFACTOR_PRECISION + '\0'
	        mpfr_sprintf (buffer, KFACTOR_FORMAT, kfactor);

		string kfactor_str = string(buffer);		
		string args = "method=KCM"				\
			      " signedIn=0"				\
			      " msbIn=1"				\
			      " lsbIn=" + to_string(lsbIn - guard + 2) +
			      " lsbOut=" + to_string(lsbOut) +
			      " constant=" + kfactor_str;
			      

		newInstance("FixRealConstMult", "kfactorDivider",
					        args,
						"X=>RK",
						"R=>Rm");
		vhdl << tab << "R <= Rm" << range(wOut-1, 0) << ";" << endl;
	}

	/* TODO: No optimization */
	void Fix2DNorm::buildCordic (int maxIterations) {
		int wIn = getWIn();
		int wOut = getWOut();
		int sizeX = wIn + guard;
		int sizeY = sizeX;

		/* CORDIC initialisation */
		/* X_0 */
		vhdl << tab << declare("X0", sizeX) << " <= \"00\" & X & " << zg(sizeX - wIn - 2) << ";" << endl;
		/* Y_0 */
		vhdl << tab << declare("Y0", sizeY) << " <= \"00\" & Y & " << zg(sizeY - wIn - 2) << ";" << endl;

		
		/* First iteration */
		vhdl << tab << "--- Iteration 1 : sign is known positive ---" << endl;
		/* X_1 */
		vhdl << tab << declare(getTarget()->adderDelay(sizeX), "X1", sizeX) << " <= X0 + Y0;" << endl;
		/* Y_1 */
		vhdl << tab << declare(getTarget()->adderDelay(sizeY), "Y1", sizeY) << " <= Y0 - X0;"  << endl;				

		
	        int stage;
		for (stage = 1; stage<=maxIterations; stage++, sizeY--) {
			REPORT(DEBUG, "stage=" << stage);
			vhdl << tab << "--- Iteration " << stage + 1 << " ---" << endl;

			/* sign(Y_i) */
			vhdl << tab << declare(join("sgnY", stage)) << " <= " << join("Y", stage) << of(sizeY - 1) << ";" << endl;

			/* X_{i+1} */
			if (sizeX - 1 >= sizeY - stage && sizeY - 1 >= stage) { 
				vhdl << tab << declare(join("YShift", stage), sizeX) << " <= "
					    << rangeAssign(sizeX - 1, sizeY - stage, join("sgnY", stage))   /* sign extension and format conversion */
					    << " & Y" << stage << range(sizeY - 1, stage) << ";" << endl;

				vhdl << tab << declare(getTarget()->fanoutDelay(sizeX+1) + getTarget()->adderDelay(sizeX), join("X", stage+1), sizeX) << " <= "
					    << join("X", stage) << " - " << join("YShift", stage) << " when " << join("sgnY", stage) << "=\'1\'     else "
					    << join("X", stage) << " + " << join("YShift", stage) << ";" << endl;
			} else {
				vhdl << tab << declare(join("X", stage+1), sizeX) << " <= " << join("X", stage) << ";" << endl;
			}
			
			/* Y_{i+1} */
			vhdl << tab << declare(join("XShift", stage), sizeY) << " <= '0' & X" << stage << range(sizeX - 1, stage) << ";" <<endl;			

			vhdl << tab << declare(join("YY", stage + 1), sizeY) << " <= "
				    << join("Y", stage) << " + " << join("XShift", stage) << " when " << join("sgnY", stage) << "=\'1\'     else "
				    << join("Y", stage) << " - " << join("XShift", stage) << ";" << endl;
			
			vhdl << tab << declare(join("Y", stage+1), sizeY-1) << " <= " << join("YY", stage+1) << range(sizeY - 2, 0) << ";" <<endl;
		}

		vhdl << tab << declare("RK", sizeX) << " <= X" << stage <<  ";" << endl;
	}
	
	void Fix2DNorm::initKFactor () {
		int wOut = getWOut();
		int maxIterations = wOut - 1;
		mpfr_t temp;
		
		mpfr_init2 (kfactor, 50*wOut);
		mpfr_init2 (temp, 50*wOut);

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

	// TODO: Compute guard bits exactly
	void Fix2DNorm::computeGuardBits () {
		guard = 6;
	}
  
	Fix2DNorm::~Fix2DNorm () {
		mpfr_clear (kfactor);
	}

	void Fix2DNorm::emulate (TestCase * tc) {
	        int wIn = getWIn();
		int wOut = getWOut();
		mpfr_t x, y, r;
		mpfr_init2 (x, 10*wIn);
		mpfr_init2 (y, 10*wIn);
		mpfr_init2 (r, 10*wOut);
  
		mpz_class rz;

		/* Get I/O values */
		mpz_class svX = tc->getInputValue ("X");
		mpz_class svY = tc->getInputValue ("Y");
		
		mpfr_set_z (x, svX.get_mpz_t(), MPFR_RNDN); // exact
		mpfr_mul_2si (x, x, lsbIn, MPFR_RNDN);      // exact
		mpfr_set_z (y, svY.get_mpz_t(), MPFR_RNDN); // exact
	        mpfr_mul_2si (y, y, lsbIn, MPFR_RNDN);      // exact
		
		/* Euclidean norm */
		mpfr_hypot(r, x, y, MPFR_RNDN);

		/* Convert r to fix point */
		mpfr_add_d (r, r, 6.0, MPFR_RNDN);
		mpfr_mul_2si (r, r, -lsbOut, MPFR_RNDN); // exact scaling

		mpz_class mask = (mpz_class(1) << wOut) - 1;
		
		/* Rounding down */
		mpfr_get_z (rz.get_mpz_t(), r, MPFR_RNDD); // there can be a real rounding here
		rz -= mpz_class(6) << -lsbOut;
		rz &= mask;
		tc->addExpectedOutput ("R", rz);

		/* Rounding up */
		mpfr_get_z (rz.get_mpz_t(), r, MPFR_RNDU); // there can be a real rounding here
		rz -= mpz_class(6) << -lsbOut;
		rz &= mask;
		tc->addExpectedOutput ("R", rz);

		/* clean up */
		mpfr_clears (x, y, r, NULL);
	}

	void Fix2DNorm::buildStandardTestCases (TestCaseList * tcl) {
		TestCase* tc;
		
		/* N(0, 0) = 0 */
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(0));
		tc -> addInput ("Y", mpz_class(0));
		emulate(tc);
		tcl->add(tc);

		/* Define 0.999_ */
		int wIn = getWIn();
		mpz_class max = (mpz_class(1) << wIn) - mpz_class(1);

		/* N(0.999_, 0) = 0.999_ */
		tc = new TestCase (this);
		tc -> addInput ("X", max);
		tc -> addInput ("Y", mpz_class(0));
		emulate(tc);
		tcl->add(tc);

		/* N(0, 0.999_) = 0.999_ */
		tc = new TestCase (this);
		tc -> addInput ("X", mpz_class(0));
		tc -> addInput ("Y", max);
		emulate(tc);
		tcl->add(tc);
		
		/* N(0.999_, 0.999_) ~ sqrt(2) */
		tc = new TestCase (this);
		tc -> addInput ("X", max);
		tc -> addInput ("Y", max);
		emulate(tc);
		tcl->add(tc);
	}
	
	OperatorPtr Fix2DNorm::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
		int lsbIn, lsbOut, method;
		UserInterface::parseInt(args, "lsbIn",  &lsbIn);
		UserInterface::parseInt(args, "lsbOut", &lsbOut);
		UserInterface::parseInt(args, "method", &method);
		//select the method
		return new Fix2DNorm(parentOp, target, lsbIn, lsbOut);
			
	}

	void Fix2DNorm::registerFactory() {
		UserInterface::add("Fix2DNorm", // name
				     "Computes sqrt(x*x+y*y)",
				     "CompositeFixPoint",
				     "", // seeAlso
				     "lsbIn(int): weight of the LSB of input;"   \
				     "lsbOut(int): weight of the LSB of output;" \
				     "method(int)=-1: technique to use, -1 selects a sensible default",
				     "",
				     Fix2DNorm::parseArguments);
	}
}

