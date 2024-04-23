/*
  An FP exponential for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/
#include <cmath> // For NaN
#include <fstream>
#include <iostream>
#include <sstream>

#include "flopoco/ConstMult/FixRealKCM.hpp"
#include "flopoco/ExpLog/Exp.hpp"
#include "flopoco/FixFunctions/FixFunctionByPiecewisePoly.hpp"
#include "flopoco/FixFunctions/FixFunctionByTable.hpp"
#include "flopoco/IntAddSubCmp/IntAdder.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/ShiftersEtc/Shifters.hpp"
#include "flopoco/Tables/DiffCompressedTable.hpp"
#include "flopoco/Tables/TableOperator.hpp"
#include "flopoco/TestBenches/FPNumber.hpp"
#include "flopoco/utils.hpp"

using namespace std;



/* TODOs
Obtaining 400MHz in Exp 8 23 depends on the version of ISE. Test with recent one.
remove the nextCycle after the multiplier

check the multiplier in the case 8 27: logic only, why?

Pass DSPThreshold to PolyEval

replace the truncated mult and following adder with an FixedMultAdd
Clean up poly eval and bitheapize it


All the tables could be FixFunctionByTable...
*/

#define USE_FIX_FUNCTION_BY_TABLE

#define LARGE_PREC 1000 // 1000 bits should be enough for everybody

namespace flopoco{

	//Obsolete ?
	vector<mpz_class>	Exp::magicTable(int sizeExpA, int sizeExpZPart, bool storeExpZmZm1)
		//	DualTable(target, 9, sizeExpA_+sizeExpZPart_, 0, 511),
	{
		vector<mpz_class> result;
		int wIn=9;
		for(int x=0; x<512; x++){
			mpz_class h, l;
			mpfr_t a, yh,yl, one;

			// convert x to 2's complement
			int xs=x;
			if(xs>>(wIn-1))
				xs-=(1<<wIn);

			mpfr_init2(a, wIn);
			mpfr_init2(one, 16);
			mpfr_set_d(one, 1.0, GMP_RNDN);
			mpfr_init2(yh, LARGE_PREC);
			mpfr_init2(yl, LARGE_PREC);


			// First build e^a
			mpfr_set_si(a, xs, GMP_RNDN);
			mpfr_div_2si(a, a, wIn, GMP_RNDN); // now a in [-1/2, 1/2[
			mpfr_exp(yh, a, GMP_RNDN); // e^x in [0.6,1.7[

			mpfr_mul_2si(yh, yh, sizeExpA-1, GMP_RNDN); 		// was 26
			mpfr_get_z(h.get_mpz_t(), yh,  GMP_RNDN);  // Here is the rounding! Should be a 27-bit number


			// build z in a
			mpfr_set_ui(a, x, GMP_RNDN);
			mpfr_div_2si(a, a, 2*wIn, GMP_RNDN); // now a in [0,1[. 2^-9

			// now build e^z part

			mpfr_exp(yl, a, GMP_RNDN); // e^(2^-9 z)
			if(storeExpZmZm1)
				mpfr_sub(yl, yl, a, GMP_RNDN); // e^(2^-9 x) -x
			mpfr_sub(yl, yl, one, GMP_RNDN); // e^(2^-9 x) -x -1 or e^(2^-9 x) -1, depending on the case

			//now scale to align LSB with expA
			mpfr_mul_2si(yl, yl, sizeExpA-1, GMP_RNDN);
			mpfr_get_z(l.get_mpz_t(), yl,  GMP_RNDN);

			// debug
			if((h>=(1<<27)) || l>=512 || h<0 || l<0)
				REPORT(LogLevel::MESSAGE, "Ouch!!!!!" <<"x=" << x << " " << xs << "    " << h << " " << l );

			//cout << x << "\t" << h << "\t" << l <<endl;
			mpfr_clears(yh, yl, a, one, NULL);

			result.push_back(l + (h<<sizeExpZPart));
		}
		return result;
	};


	// All these functions could be removed by calling FixFunctionByTable
	// it is somehow pedagogical, too
	vector<mpz_class>	tableExpZm1(int k, int l) // with the notations of the ASA book: l=-wF-g
	{
		// This table inputs Z in the format (-k-1, l) -- sizeZ and outputs e^Z-1 with LSB l
		vector<mpz_class> result;
		int wIn=-l-k;
		int wOut=-l-k+1;
		for(int z=0; z<(1<<wIn); z++){
			mpz_class h;
			mpfr_t mpz, mpy, one;

			mpfr_init2(mpz, wIn);
			mpfr_init2(one, 16);
			mpfr_set_d(one, 1.0, GMP_RNDN);
			mpfr_init2(mpy, LARGE_PREC);

			// build z
			mpfr_set_ui(mpz, z, GMP_RNDN); // exact
			mpfr_mul_2si(mpz, mpz, l, GMP_RNDN); // exact
			// compute its exp
			mpfr_exp(mpy, mpz, GMP_RNDN); // rounding here
			//subtract 1
			mpfr_sub(mpy, mpy, one, GMP_RNDN); // exact e^z-1

			//now scale to an int
			mpfr_mul_2si(mpy,mpy, -l, GMP_RNDN);
			mpfr_get_z(h.get_mpz_t(), mpy,  GMP_RNDN);

			// debug
			//			if((h>=(1<<27)) || l>=512 || h<0 || l<0)
			// cout  <<"z=" << z << " h=" << h <<endl;

			mpfr_clears(mpz, mpy, one, NULL);

			result.push_back(h);
		}
		return result;
	};



	vector<mpz_class>	tableExpZmZm1(int k, int p, int l) // with the notations of the ASA book
	{
		vector<mpz_class> result;
		int wIn=-k-p;
		for(int x=0; x<(1<<wIn); x++){
			mpz_class h;
			mpfr_t mpz, mpy, one;

			mpfr_init2(mpz, wIn);
			mpfr_init2(one, 16);
			mpfr_set_d(one, 1.0, GMP_RNDN);
			mpfr_init2(mpy, LARGE_PREC);

			// build z
			mpfr_set_ui(mpz, x, GMP_RNDN);
			mpfr_div_2si(mpz, mpz, -p, GMP_RNDN);
			// compute its exp
			mpfr_exp(mpy, mpz, GMP_RNDN);
			mpfr_sub(mpy, mpy, one, GMP_RNDN); // e^z-1
			mpfr_sub(mpy, mpy, mpz, GMP_RNDN); // e^z-1 -z

			//now scale to an int
			mpfr_mul_2si(mpy,mpy, -l, GMP_RNDN);
			mpfr_get_z(h.get_mpz_t(), mpy,  GMP_RNDN);

			// debug
			// cout  <<"x=" << x << " h=" << h <<endl;

			//cout << x << "\t" << h << "\t" << l <<endl;
			mpfr_clears(mpz, mpy, one, NULL);

			result.push_back(h);
		}
		return result;
	};



	vector<mpz_class>	Exp::ExpATable(int wIn, int wOut)	{
		vector<mpz_class> result;
		for(int x=0; x<(1<<wIn); x++){
			mpz_class h;
			mpfr_t a, y;

			// convert x to 2's compliment
			int xs=x;
			if(xs>>(wIn-1))
				xs-=(1<<wIn);

			mpfr_init2(a, wIn);
			mpfr_set_si(a, xs, GMP_RNDN);
			mpfr_div_2si(a, a, wIn, GMP_RNDN); // now a in [-1/2, 1/2[
			mpfr_init2(y, LARGE_PREC);
			mpfr_exp(y, a, GMP_RNDN); // in [0.6, 1.7], MSB position is 0

			mpfr_mul_2si(y, y, wOut-1, GMP_RNDN);
			mpfr_get_z(h.get_mpz_t(), y,  GMP_RNDN);  // here the rounding takes place

			// debug
			if((h>=(mpz_class(1)<<wOut)) || h<0)
				REPORT(LogLevel::MESSAGE, "Ouch!!!!!" << h);

			//cout << x << "\t" << h << "\t" << l <<endl;
			mpfr_clears(y, a, NULL);

			result.push_back(h);
		}
		return result;
	};


	Exp::Exp(
							 OperatorPtr parentOp, Target* target,
							 int wE_, int wF_,
							 int k_, int d_, int guardBits, bool fullInput
							 ):
		Operator(parentOp, target),
		wE(wE_),
		wF(wF_)
	{
		// Paperwork

		ostringstream name;
		name << "Exp_" << wE << "_" << wF ;
		setNameWithFreqAndUID(name.str());

		setCopyrightString("F. de Dinechin, Bogdan Pasca (2008-2021)");
		srcFileName="Exp";

		int blockRAMSize = getTarget()->sizeOfMemoryBlock();

		ExpArchitecture* myExp = new ExpArchitecture(blockRAMSize, wE_, wF_, k_, d_, guardBits, fullInput);

		// Various architecture parameter to be determined before attempting to
		// build the architecture
		this->k = myExp->getk();
		this->d = myExp->getd();
		this->g = myExp->getg();

		bool expYTabulated = myExp->getExpYTabulated();
		bool useTableExpZm1 = myExp->getUseTableExpZm1();
		bool useTableExpZmZm1 = myExp->getUseTableExpZmZm1();
		int sizeY = myExp->getSizeY();
		int sizeZ = myExp->getSizeZ();
		int sizeExpY = myExp->getSizeExpY();
		int sizeExpA = myExp->getSizeExpA();
		int sizeZtrunc = myExp->getSizeZtrunc();
		int sizeExpZmZm1 = myExp->getSizeExpZmZm1();
		int sizeExpZm1 = myExp->getSizeExpZm1();
		int sizeMultIn = myExp->getSizeMultIn();

		// nY is in [-1/2, 1/2]

		int wFIn; // The actual size of the input
		if(fullInput)
			wFIn=wF+wE+g;
		else
			wFIn=wF;

		addFPInput("X", wE, wFIn);
		addFPOutput("R", wE, wF);


		//********* Building a few MPFR constants, useful or obsolete *********
		mpz_class mpzLog2, mpzInvLog2;

		mpfr_t mp2, mp1, mplog2, mpinvlog2;

		mpfr_inits(mp1, mp2, NULL);
		mpfr_set_si(mp1, 1, GMP_RNDN);
		mpfr_set_si(mp2, 2, GMP_RNDN);

		// 1/log2 ~ 1.44, truncated, on sizeFirstKCM bits
		int sizeFirstKCM=wE+4;

		// All this mostly useless now that we have FixReal KCM
		mpfr_init2(mplog2, 3*(wE+wF+g));	// way too much precision
		mpfr_log(mplog2, mp2, GMP_RNDN);
		mpfr_init2(mpinvlog2, sizeFirstKCM);
		mpfr_div(mpinvlog2, mp1, mplog2, GMP_RNDN);
		mpfr_mul_2si(mpinvlog2, mpinvlog2, sizeFirstKCM-1, GMP_RNDN); //Exact
		mpfr_get_z(mpzInvLog2.get_mpz_t(), mpinvlog2, GMP_RNDN);
		mpfr_clear(mplog2);

		//Computing log2 ~ 0.69 on wE+wF+g bits, rounded down, too
		mpfr_init2(mplog2, wE+wF+g);
		mpfr_log(mplog2, mp2, GMP_RNDN);
		mpfr_mul_2si(mplog2, mplog2, wE+wF+g, GMP_RNDN); //Exact
		mpfr_get_z(mpzLog2.get_mpz_t(), mplog2, GMP_RNDN);

		mpfr_clears(mp1, mp2, mplog2, mpinvlog2, NULL);

		addConstant("wE", "positive", wE);
		addConstant("wF", "positive", wF);
		addConstant("wFIn", "positive", wFIn);
		addConstant("g", "positive", g);

		int bias = (1<<(wE-1))-1;
		if(bias < wF+g){
			ostringstream e;
			e << "ERROR in Exp, unable to build architecture if wF+g > 2^(wE-1)-1." <<endl;
			e << "      Try increasing wE." << endl;
			e << "      If you really need Exp to work with such values, please report this as a bug :)" << endl;
			throw e.str();
		}



		//******** Input unpacking and shifting to fixed-point ********

		vhdl << tab  << declare("Xexn", 2) << " <= X(wE+wFIn+2 downto wE+wFIn+1);" << endl;
		vhdl << tab  << declare("XSign") << " <= X(wE+wFIn);" << endl;
		vhdl << tab  << declare("XexpField", wE) << " <= X(wE+wFIn-1 downto wFIn);" << endl;
		vhdl << tab  << declareFixPoint("Xfrac", false, -1,-wFIn) << " <= unsigned(X(wFIn-1 downto 0));" << endl;

		int e0 = bias - (wF+g);
		vhdl << tab  << declare("e0", wE+2) << " <= conv_std_logic_vector(" << e0 << ", wE+2);  -- bias - (wF+g)" << endl;
		vhdl << tab  << declare(getTarget()->adderDelay(wE+2), "shiftVal", wE+2) << " <= (\"00\" & XexpField) - e0; -- for a left shift" << endl;

		vhdl << tab  << "-- underflow when input is shifted to zero (shiftval<0), in which case exp = 1" << endl;
		vhdl << tab  << declare("resultWillBeOne") << " <= shiftVal(wE+1);" << endl;

		// As we don't have a signed shifter, shift first, complement next. TODO? replace with a signed shifter
		vhdl << tab << "--  mantissa with implicit bit" << endl;
		vhdl << tab  << declareFixPoint("mXu", false, 0,-wFIn) << " <= \"1\" & Xfrac;" << endl;

		// left shift
		vhdl << tab  << "-- Partial overflow detection" << endl;
		int maxshift = wE-2+ wF+g; // maxX < 2^(wE-1);
		vhdl << tab  << declare("maxShift", wE+1) << " <= conv_std_logic_vector(" << maxshift << ", wE+1);  -- wE-2 + wF+g" << endl;
		vhdl << tab  << declare(getTarget()->adderDelay(wE+1),"overflow0") << " <= not shiftVal(wE+1) when shiftVal(wE downto 0) > maxShift else '0';" << endl;

		int shiftInSize = intlog2(maxshift);
		vhdl << tab  << declare("shiftValIn", shiftInSize) << " <= shiftVal" << range(shiftInSize-1, 0) << ";" << endl;
		newInstance("Shifter",
								"mantissa_shift",
								"wX=" + to_string(wFIn+1) + " maxShift=" + to_string(maxshift) + " dir=0",
								"X=>mXu,S=>shiftValIn",
								"R=>fixX0");

		int sizeShiftOut=maxshift + wFIn+1;
		int sizeXfix = wE-2 +wF+g +1; // still unsigned; msb=wE-1; lsb = -wF-g

		vhdl << tab << declareFixPoint("ufixX", false, wE-2, -wF-g) << " <= " << " unsigned(fixX0" <<
			range(sizeShiftOut -1, sizeShiftOut- sizeXfix) <<
			") when resultWillBeOne='0' else " << zg(sizeXfix) <<  ";" << endl;
#if 0
		// TODO here it doesn't match exactly the error analysis in the ASA Book, but it works
		int lsbXforFirstMult=-3;
		int msbXforFirstMult = wE-2;
		int sizeXMulIn = msbXforFirstMult - lsbXforFirstMult +1; // msb=wE-2, lsb=-3
		vhdl << tab <<	declare("xMulIn", sizeXMulIn) << " <=  std_logic_vector(ufixX" <<
			range(sizeXfix-2, sizeXfix - sizeXMulIn-1  ) <<
			"); -- truncation, error 2^-3" << endl;
#else
		int lsbXforFirstMult = -3;
		int msbXforFirstMult = wE-2;
		resizeFixPoint("xMulIn", "ufixX", msbXforFirstMult, lsbXforFirstMult);
#endif

		//***************** Multiplication by 1/log2 to get approximate result ********

		newInstance("FixRealKCM",
								"MulInvLog2",
								 // unsigned here, the conversion to signed comes later
								 "signedIn=0 msbIn=" + to_string(msbXforFirstMult)
								+ " lsbIn=" + to_string(lsbXforFirstMult)
								+ " lsbOut=0"
								+ " constant=1/log(2)"
								+ " targetUlpError=" + to_string(0.5 + 0.12), // we have 0.125 on X, and target is 0.5+0.22
								"X=>xMulIn",
								"R=>absK");



		// Now I have two things to do in parallel: compute K, and compute absKLog2
		// First compute K
		vhdl << tab << declare(getTarget()->adderDelay(wE+1),
													 "minusAbsK",wE+1) << " <= " << rangeAssign(wE, 0, "'0'")<< " - ('0' & absK);"<<endl;
		// The synthesizer should be able to merge the addition and this mux,
		vhdl << tab << declare("K",wE+1) << " <= minusAbsK when  XSign='1'   else ('0' & absK);"<<endl;


		// TODO here! We do not need to compute the leading WE bits, as they will be cancelled out by the subtraction anyway.
		// Not sure the fixrealKCM interface is ready
		newInstance("FixRealKCM",
								"MulLog2",
								// unsigned here, the conversion to signed comes later
								 "signedIn=0 msbIn=" + to_string(wE-1)
								+ " lsbIn=0"
								+ " lsbOut=" + to_string(-wF-g)
								+ " constant=log(2)"
								+ " targetUlpError=1.0",
								"X=>absK",
								"R=>absKLog2");



		// absKLog2: msb wE-2, lsb -wF-g

		sizeY=wF+g; // This is also the weight of Y's LSB

		vhdl << tab << declare(getTarget()->logicDelay(), "subOp1",sizeY) << " <= std_logic_vector(ufixX" << range(sizeY-1, 0) << ") when XSign='0'"
				 << " else not (std_logic_vector(ufixX" << range(sizeY-1, 0) << "));"<<endl;
		vhdl << tab << declare("subOp2",sizeY) << " <= absKLog2" << range(sizeY-1, 0) << " when XSign='1'"
				 << " else not (absKLog2" << range(sizeY-1, 0) << ");"<<endl;

		newInstance("IntAdder",
								"theYAdder",
								"wIn=" + to_string(sizeY),// we know the leading bits will cancel out
								"X=>subOp1,Y=>subOp2",
								"R=>Y",
								"Cin=>'1'"); // it is a subtraction


		vhdl << tab << "-- Now compute the exp of this fixed-point value" <<endl;



		if(expYTabulated) {

#ifdef USE_FIX_FUNCTION_BY_TABLE // both work, not sure which is the simplest to read
			// tabulate e^Y with Y in sfix(-1, -wF-g-2) ie in -0.5, 0.5.
			newInstance("FixFunctionByTable",
									"ExpYTable",
									"f=exp(x*1b" + to_string(-1) + ") signedIn=true lsbIn=" + to_string(-wF-g - (-1)) + " lsbOut="+to_string(-wF-g-2), // -2 because sizeExpY has a "free" +2
									"X=>Y",
									"Y=>expY");
#else
			vector<mpz_class> expYTableContent = ExpATable(sizeY, sizeExpY);
			TableOperator::newUniqueInstance(this, "Y", "expY",
															 expYTableContent,
															 "ExpYTable",
															 sizeY, sizeExpY);
#endif
		}

		else{ // expY not plainly tabulated, splitting it into A and Z
			vhdl << tab << declare("A", k) << " <= Y" << range(sizeY-1, sizeY-k) << ";\n";
			vhdl << tab << declare("Z", sizeZ) << " <= Y" << range(sizeZ-1, 0) << ";\n";

#ifdef USE_FIX_FUNCTION_BY_TABLE
			// tabulate e^Y with Y in sfix(-1, -wF-g) ie in -0.5, 0.5.
			// using compression
			bool compression = getTarget()->tableCompression();
			getTarget()->setTableCompression(true);
			newInstance("FixFunctionByTable",
									"ExpATable",
									"f=exp(x*1b" + to_string(-1) + ") signedIn=true lsbIn=" + to_string(-k - (-1)) + " lsbOut="+to_string(-wF-g), // no -2 because sizeExpA does not have a "free" +2
									"X=>A",
									"Y=>expA");
			getTarget()->setTableCompression(compression);
#else
			vector<mpz_class> expYTableContent = ExpATable(k, sizeExpA); // e^A-1 has MSB weight 1
			if(getTarget()->tableCompression() == false) {
				TableOperator::newUniqueInstance(this, "A", "expA",
																 expYTableContent,
																 "ExpATable",
																 k, sizeExpA);
			} else {
				TableOperator::newUniqueInstance(this, "A", "expARef",
																 expYTableContent,
																 "ExpATableRef",
																 k, sizeExpA);
				auto op_ptr = reinterpret_cast<DiffCompressedTable*>(
					DiffCompressedTable::newUniqueInstance(this, "A", "expA",
																								expYTableContent,
																								"ExpATable",
																								k, sizeExpA));
				op_ptr->report_compression_gain();
				// TODO get rid of this
			}
#endif

				//should be				int p = -wF-g+k-1; as in the ASA book  TODO investigate
				int p = -wF-g+k; // ASA book notation

				if(useTableExpZm1){
#ifdef USE_FIX_FUNCTION_BY_TABLE 
					// tabulate e^Z-1 with Z in sfix(-k-1, -wF-g)
					// TODO MSB not computed correctly, check this always works
					newInstance("FixFunctionByTable",
											"ExpZm1Table",
											"f=(exp(x*1b" + to_string(-k-1) + ")-1) signedIn=true lsbIn=" + to_string(-wF-g -(-k-1)) + " lsbOut="+to_string(-wF-g),
											"X=>Z",
											"Y=>expZm1_p");
					// test size of sortie
					vhdl << declare(0., "expZm1", sizeZ+1) << " <= \"0\" & expZm1_p;" << endl;
#else
					vector<mpz_class> expZm1TableContent = tableExpZm1(k, -wF-g);
					TableOperator::newUniqueInstance(this, "Z", "expZm1",
																	 expZm1TableContent,
																	 "ExpZm1Table",
																	 sizeZ,
																	 sizeZ+1);		
#endif
				}

				else if (useTableExpZmZm1)  {
					vhdl << tab << declare("Ztrunc", sizeZtrunc) << " <= Z" << range(sizeZ-1, sizeZ-sizeZtrunc) << ";\n";
#ifdef USE_FIX_FUNCTION_BY_TABLE 
					// tabulate e^Z-1-Z with Z in ufix(-k, p)
					// TODO understand why in the book it is ufix(-k-1, p) and here it isn't
					newInstance("FixFunctionByTable",
											"ExpZmZm1Table",
											"f=exp(x*1b" + to_string(-k) + ")-1-x*1b" + to_string(-k) + " signedIn=false lsbIn=" + to_string(p-(-k)) + " lsbOut="+to_string(-wF-g),
											"X=>Ztrunc",
											"Y=>expZmZm1");

#else
					vector<mpz_class> expYTableContent = tableExpZmZm1(k, p, -wF-g);
					TableOperator::newUniqueInstance(this, "Ztrunc", "expZmZm1",
																	 expYTableContent,
																	 "ExpZmZm1Table",
																	 -k-p,
																	 -2*k+wF+g);
#endif
				}



				else { // generic case, use a polynomial evaluator

					vhdl << tab << declare("Ztrunc", sizeZtrunc) << " <= Z" << range(sizeZ-1, sizeZ-sizeZtrunc) << ";\n";
					REPORT(LogLevel::MESSAGE, "Generating the polynomial approximation, this may take some time");
					// We want the LSB value to be  2^(wF+g)
					ostringstream function;
					function << "1b"<<2*k-1<<"*(exp(x*1b-" << k << ")-x*1b-" << k << "-1)";  // e^z-z-1
					newInstance("FixFunctionByPiecewisePoly",
											"poly",
											+"f=" + function.str() + ""
											+" lsbIn=" + to_string(-sizeZtrunc)
											+" lsbOut=" + to_string(-wF-g+2*k-1)
											+" d=" + to_string(d),
											"X=>Ztrunc",
											"Y=>expZmZm1");

			}// end if table/poly

			// Do we need the adder that adds back Z to e^Z-Zm1?
			if(!useTableExpZm1) {
				// here we have in expZmZm1 e^Z-Z-1
				// Alignment of expZmZm10:  MSB has weight -2*k, LSB has weight -(wF+g).
				//		vhdl << tab << declare("ShouldBeZero2", (sizeExpY-
				//		sizeExpZmZm1)) << " <= expZmZm1_0" << range(sizeExpY-1,
				//		sizeExpZmZm1)  << "; -- for debug to check it is always
				//		0" <<endl;

				vhdl << tab << "-- Computing Z + (exp(Z)-1-Z)" << endl;

				vhdl << tab << declare( "expZm1adderX", sizeExpZm1) <<
				" <= '0' & Z;"<<endl;

				int sizeActualexpZmZm1 = getSignalByName("expZmZm1")->width(); // if faithful it will be one bit more... be on the safe size
				vhdl << tab << declare( "expZm1adderY", sizeExpZm1) << " <= " <<
					rangeAssign(sizeExpZm1-sizeActualexpZmZm1-1, 0, "'0'") << " & expZmZm1 ;" << endl;

		newInstance("IntAdder",
								"Adder_expZm1",
								"wIn=" + to_string(sizeExpZm1),
								"X=>expZm1adderX,Y=>expZm1adderY",
								"R=>expZm1",
								"Cin=>'0'");


			} // now we have in expZm1 e^Z-1

			// Now, if we want g=3 (needed for the magic table to fit a BRAM for single prec)
			// we need to keep max error below 4 ulp.
			// Every half-ulp counts, in particular we need to round expA instead of truncating it...
			// The following "if" is because I have tried several alternatives to get rid of this addition.
			if(useTableExpZm1 || useTableExpZmZm1) {
				vhdl << tab << "-- Rounding expA to the same accuracy as expZm1" << endl;
				vhdl << tab << "--   (truncation would not be accurate enough and require one more guard bit)" << endl;
				vhdl << tab << declare("expA_T", sizeMultIn+1) << " <= expA"+range(sizeExpA-1, sizeExpA-sizeMultIn-1) << ";" << endl;
				newInstance("IntAdder",
										"Adder_expArounded0",
										"wIn=" + to_string(sizeMultIn+1),
										"X=>expA_T",
										"R=>expArounded0",
										"Cin=>'1',Y=>" +  zg(sizeMultIn+1,0) ); // two constant inputs

				vhdl << tab << declare("expArounded", sizeMultIn) << " <= expArounded0" << range(sizeMultIn, 1) << ";" << endl;
			}
			else{ // if  generic we have a faithful expZmZm1, not a CR one: we need g=4, so anyway we do not need to worry
				vhdl << tab << "-- Truncating expA to the same accuracy as expZm1" << endl;
				vhdl << tab << declare("expArounded", sizeMultIn) << " <= expA" << range(sizeExpA-1, sizeExpA-sizeMultIn) << ";" << endl;
			}

#if 0 // full product, truncated
			int sizeProd;
			sizeProd = sizeMultIn + sizeExpZm1;
			Operator* lowProd;
			lowProd = new IntMultiplier(target, sizeMultIn, sizeExpZm1,
			                            0,  // untruncated
			                            false  /*unsigned*/
			                            );
			addSubComponent(lowProd);

			inPortMap(lowProd, "X", "expArounded");
			inPortMap(lowProd, "Y", "expZm1");
			outPortMap(lowProd, "R", "lowerProduct");

			vhdl << instance(lowProd, "TheLowerProduct")<<endl;
			vhdl << tab << declare("extendedLowerProduct",sizeExpY) << " <= (" << rangeAssign(sizeExpY-1, sizeExpY-k+1, "'0'")
			     << " & lowerProduct" << range(sizeProd-1, sizeProd - (sizeExpY-k+1)) << ");" << endl;


#else // using a truncated multiplier

			     int sizeProd;
			     sizeProd = sizeExpZm1+1;
					 newInstance("IntMultiplier",
											 "TheLowerProduct",
											 "wX=" + to_string(sizeMultIn)
											 +" wY=" + to_string(sizeExpZm1)
											 +" wOut=" + to_string(sizeProd)   // truncated
											 +" signedI0=0",
											 "X=>expArounded, Y=>expZm1 ",
											 "R=>lowerProduct");


			vhdl << tab << declare("extendedLowerProduct",sizeExpY) << " <= (" << rangeAssign(sizeExpY-1, sizeExpY-k+1, "'0'")
			<< " & lowerProduct" << range(sizeProd-1, 0) << ");" << endl;

#endif

			vhdl << tab << "-- Final addition -- the product MSB bit weight is -k+2 = "<< -k+2 << endl;
			// remember that sizeExpA==sizeExpY
			newInstance("IntAdder",
									"TheFinalAdder",
									"wIn=" + to_string(sizeExpY),
									"X=>expA,Y=>extendedLowerProduct",
									"R=>expY",
									"Cin=>'0'");

		} // end if(expYTabulated)


		// The following is generic normalization/rounding code if we have in expY an approx of exp(y) of size 	sizeExpY
		// with MSB of weight 2^1
		// We start a cycle here
//		nextCycle();

		vhdl << tab << declare("needNoNorm") << " <= expY(" << sizeExpY-1 << ");" << endl;
		vhdl << tab << "-- Rounding: all this should consume one row of LUTs" << endl;
		vhdl << tab << declare(getTarget()->logicDelay(), "preRoundBiasSig", wE+wF+2)
		<< " <= conv_std_logic_vector(" << bias << ", wE+2)  & expY" << range(sizeExpY-2, sizeExpY-2-wF+1) << " when needNoNorm = '1'" << endl
		<< tab << tab << "else conv_std_logic_vector(" << bias-1 << ", wE+2)  & expY" << range(sizeExpY-3, sizeExpY-3-wF+1) << " ;" << endl;

		vhdl << tab << declare("roundBit") << " <= expY(" << sizeExpY-2-wF << ")  when needNoNorm = '1'    else expY(" <<  sizeExpY-3-wF << ") ;" << endl;
		vhdl << tab << declare("roundNormAddend", wE+wF+2) << " <= K(" << wE << ") & K & "<< rangeAssign(wF-1, 1, "'0'") << " & roundBit;" << endl;


		newInstance("IntAdder",
								"roundedExpSigOperandAdder",
								"wIn=" + to_string(wE+wF+2),
								"X=>preRoundBiasSig,Y=>roundNormAddend",
								"R=>roundedExpSigRes",
								"Cin=>'0'");
		vhdl << tab << declare(getTarget()->logicDelay(), "roundedExpSig", wE+wF+2) << " <= roundedExpSigRes when Xexn=\"01\" else "
		<< " \"000\" & (wE-2 downto 0 => '1') & (wF-1 downto 0 => '0');" << endl;

		vhdl << tab << declare(getTarget()->logicDelay(), "ofl1") << " <= not XSign and overflow0 and (not Xexn(1) and Xexn(0)); -- input positive, normal,  very large" << endl;
		vhdl << tab << declare("ofl2") << " <= not XSign and (roundedExpSig(wE+wF) and not roundedExpSig(wE+wF+1)) and (not Xexn(1) and Xexn(0)); -- input positive, normal, overflowed" << endl;
		vhdl << tab << declare("ofl3") << " <= not XSign and Xexn(1) and not Xexn(0);  -- input was -infty" << endl;
		vhdl << tab << declare("ofl") << " <= ofl1 or ofl2 or ofl3;" << endl;

		vhdl << tab << declare("ufl1") << " <= (roundedExpSig(wE+wF) and roundedExpSig(wE+wF+1))  and (not Xexn(1) and Xexn(0)); -- input normal" << endl;
		vhdl << tab << declare("ufl2") << " <= XSign and Xexn(1) and not Xexn(0);  -- input was -infty" << endl;
		vhdl << tab << declare("ufl3") << " <= XSign and overflow0  and (not Xexn(1) and Xexn(0)); -- input negative, normal,  very large" << endl;

		vhdl << tab << declare("ufl") << " <= ufl1 or ufl2 or ufl3;" << endl;

		vhdl << tab << declare("Rexn", 2) << " <= \"11\" when Xexn = \"11\"" << endl
		<< tab << tab << "else \"10\" when ofl='1'" << endl
		<< tab << tab << "else \"00\" when ufl='1'" << endl
		<< tab << tab << "else \"01\";" << endl;

		vhdl << tab << "R <= Rexn & '0' & roundedExpSig" << range(wE+wF-1, 0) << ";" << endl;

	}

	Exp::~Exp()
	{
	}



	void Exp::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class svX = tc->getInputValue("X");

		/* Compute correct value */
		FPNumber fpx(wE, wF, svX);

		mpfr_t x, ru,rd;
		mpfr_init2(x,  1+wF);
		mpfr_init2(ru, 1+wF);
		mpfr_init2(rd, 1+wF);
		fpx.getMPFR(x);
		mpfr_exp(rd, x, GMP_RNDD);
		mpfr_exp(ru, x, GMP_RNDU);
		FPNumber  fprd(wE, wF, rd);
		FPNumber  fpru(wE, wF, ru);
		mpz_class svRD = fprd.getSignalValue();
		mpz_class svRU = fpru.getSignalValue();
		tc->addExpectedOutput("R", svRD);
		tc->addExpectedOutput("R", svRU);
		mpfr_clears(x, ru, rd, NULL);
	}



	void Exp::buildStandardTestCases(TestCaseList* tcl){
		TestCase *tc;

		mpfr_t x, y;
		FPNumber *fx, *fy;
		// double d;

		mpfr_init2(x, 1+wF);
		mpfr_init2(y, 1+wF);



		tc = new TestCase(this);
		tc->addFPInput("X", log(2));
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", FPNumber::plusDirtyZero);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", FPNumber::minusDirtyZero);
		emulate(tc);
		tcl->add(tc);



		tc = new TestCase(this);
		tc->addFPInput("X", 1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", 2.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", 1.5);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -1.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -2.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addFPInput("X", -3.0);
		emulate(tc);
		tcl->add(tc);

		tc = new TestCase(this);
		tc->addComment("The largest number whose exp is finite");
		fx = new FPNumber(wE, wF, FPNumber::largestPositive);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDN);
		//		cout << "A " << fx->getSignalValue() << endl;
		//		 d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;
		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number whose exp is infinite");
		mpfr_nextabove(y);
		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);




		tc = new TestCase(this);
		tc->addComment("The last number whose exp is nonzero");
		fx = new FPNumber(wE, wF, FPNumber::smallestPositive);
		fx->getMPFR(x);
		mpfr_log(y, x, GMP_RNDU);

		// cout << "A " << fx->getSignalValue() << endl;
		// d = mpfr_get_d(x, GMP_RNDN);
		// cout << d << endl;
		// d = mpfr_get_d(y, GMP_RNDN);
		// cout << d << endl;

		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fx);

		tc = new TestCase(this);
		tc->addComment("The first number whose exp flushes to zero");
		mpfr_nextbelow(y);
		fy = new FPNumber(wE, wF, y);
		tc->addFPInput("X", fy);
		emulate(tc);
		tcl->add(tc);
		delete(fy);

		mpfr_clears(x, y, NULL);
	}





	// One test out of 8 fully random (tests NaNs etc)
	// All the remaining ones test numbers with exponents between -wF-3 and wE-2,
	// For numbers outside this range, exp over/underflows or flushes to 1.

	TestCase* Exp::buildRandomTestCase(int i){
		TestCase *tc;
		tc = new TestCase(this);
		mpz_class x;
		mpz_class normalExn = mpz_class(1)<<(wE+wF+1);
		mpz_class bias = ((1<<(wE-1))-1);
		/* Fill inputs */
		if ((i & 7) == 0) { //fully random
			x = getLargeRandom(wE+wF+3);
		}
		else
		{
				mpz_class e = (getLargeRandom(wE+wF) % (wE+wF+2) ) -wF-3; // Should be between -wF-3 and wE-2
				//cout << e << endl;
				e = bias + e;
				mpz_class sign = getLargeRandom(1);
				x  = getLargeRandom(wF) + (e << wF) + (sign<<(wE+wF)) + normalExn;
			}
			tc->addInput("X", x);
		/* Get correct outputs */
			emulate(tc);
			return tc;
		}




		OperatorPtr Exp::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
			int wE, wF, k, d, g;
			bool fullInput;
			ui.parseStrictlyPositiveInt(args, "wE", &wE);
			ui.parseStrictlyPositiveInt(args, "wF", &wF);
			ui.parsePositiveInt(args, "k", &k);
			ui.parsePositiveInt(args, "d", &d);
			ui.parseInt(args, "g", &g);
			ui.parseBoolean(args, "fullInput", &fullInput);
			return new Exp(parentOp, target, wE, wF, k, d, g, fullInput);
		}



		TestList Exp::unitTest(int testLevel)
		{
		// the static list of mandatory tests
			TestList testStateList;
			vector<pair<string,string>> paramList;
            std::vector<std::array<int, 6>> paramValues;

            paramValues = { // testing the default value on the most standard cases
                {5,  10, 0, 0, -1, 0},
                {8,  23, 0, 0, -1, 0},
                {11, 52, 0, 0, -1, 0}
            };
            if (testLevel == TestLevel::QUICK) {
                // just test paramValues
            }

        if (testLevel >= TestLevel::SUBSTANTIAL) {
            // The substantial unit tests
            // First test with plainVHDL, then with cool multipliers
        for (int wF=5; wF<53; wF+=1) {
            // test various input widths
            int nbByteWE = 6+(wF/10);
            while(nbByteWE>wF){
                nbByteWE -= 2;
            }
            paramList.push_back(make_pair("wF",to_string(wF)));
            paramList.push_back(make_pair("wE",to_string(nbByteWE)));
            paramList.push_back(make_pair("plainVHDL","true"));
            testStateList.push_back(paramList);
            paramList.clear();
        }
        for (int wF=5; wF<53; wF+=1) {
            // test various input widths
            int nbByteWE = 6+(wF/10);
            while(nbByteWE>wF){
                nbByteWE -= 2;
            }
            paramList.push_back(make_pair("wF",to_string(wF)));
            paramList.push_back(make_pair("wE",to_string(nbByteWE)));
            testStateList.push_back(paramList);
            paramList.clear();
        }
        }
      else
      {
          // finite number of random test computed out of testLevel
      }
        for (auto const params: paramValues) {
             paramList.push_back(make_pair("wE", to_string(params[0])));
             paramList.push_back(make_pair("wF", to_string(params[1])));
             paramList.push_back(make_pair("d", to_string(params[2])));
             paramList.push_back(make_pair("k", to_string(params[3])));
             paramList.push_back(make_pair("g", to_string(params[4])));
             paramList.push_back(make_pair("fullInput", to_string(params[5])));
             testStateList.push_back(paramList);
             paramList.clear();
        }
  		return testStateList;
	}

	template <>
	const OperatorDescription<Exp> op_descriptor<Exp> {
	    "Exp", // name
	    "A faithful floating-point exponential function.",
	    "ElementaryFunctions",
	    "", // seeAlso
	    "wE(int): exponent size in bits; \
         wF(int): mantissa size in bits;  \
         d(int)=0: degree of the polynomial. 0 choses a sensible default.; \
         k(int)=0: input size to the range reduction table, should be between 5 and 15. 0 choses a sensible default.;\
         g(int)=-1: number of guard bits;\
         fullInput(bool)=0: input a mantissa of wF+wE+g bits (useful mostly for FPPow)",
	    "Parameter d and k control the DSP/RamBlock tradeoff. In both "
	    "cases, a value of 0 choses a sensible default. Parameter g is "
	    "mostly for internal use.<br> For all the details, see <a "
	    "href=\"bib/flopoco.html#DinechinPasca2010-FPT\">this "
	    "article</a>."};
}





