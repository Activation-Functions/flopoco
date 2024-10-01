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

			//cerr << x << "\t" << h << "\t" << l <<endl;
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
			// cerr  <<"z=" << z << " h=" << h <<endl;

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
			// cerr  <<"x=" << x << " h=" << h <<endl;

			//cerr << x << "\t" << h << "\t" << l <<endl;
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

			//cerr << x << "\t" << h << "\t" << l <<endl;
			mpfr_clears(y, a, NULL);

			result.push_back(h);
		}
		return result;
	};


	Exp::Exp(
							 OperatorPtr parentOp, Target* target,
							 int wE_, int wF_,
							 int k_, int d_, int guardBits, bool fullInput, bool IEEEFPMode
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

		ExpArchitecture* myExp = new ExpArchitecture(blockRAMSize, wE_, wF_, k_, d_, guardBits, fullInput, IEEEFPMode);

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
		int MSB = myExp->getMSB();
		int LSB = myExp->getLSB();
		
		// TODO add possibility to change MSB and LSB

		// nY is in [-1/2, 1/2]

		int wFIn; // The actual size of the input
		if(fullInput)
			wFIn=wF+wE+g;
		else
			wFIn=wF;

		addInput("ufixX_i", MSB-LSB+1);
		addInput("XSign");

		addOutput("expY",sizeExpY);
		addOutput("K", MSB+3); // not sure this is really what we want

		//addFPInput("X", wE, wFIn);
		//addFPOutput("R", wE, wF);

		vhdl << declareFixPoint("ufixX", false, MSB, -wF-g) << " <= unsigned(ufixX_i);" << endl;

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
		int msbXforFirstMult = MSB;
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
		vhdl << tab << declare(getTarget()->adderDelay(MSB+3),
													 "minusAbsK",MSB+3) << " <= " << rangeAssign(MSB+3-1, 0, "'0'")<< " - ('0' & absK);"<<endl;
		// The synthesizer should be able to merge the addition and this mux,
		//vhdl << tab << declare("K",wE+1) << " <= minusAbsK when  XSign='1'   else ('0' & absK);"<<endl;
		vhdl << tab << "K <= minusAbsK when  XSign='1'   else ('0' & absK);"<<endl;


		// TODO here! We do not need to compute the leading WE bits, as they will be cancelled out by the subtraction anyway.
		// Not sure the fixrealKCM interface is ready
		newInstance("FixRealKCM",
								"MulLog2",
								// unsigned here, the conversion to signed comes later
								 "signedIn=0 msbIn=" + to_string(MSB+1) // always msb+1 ? TODO check
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

		//outputs are expY and K
		

	}

	Exp::~Exp()
	{
	}


/* TODO should this exp helper be emulated ? not sure
	void Exp::emulate(TestCase * tc)
	{
		
		// Get I/O values 
		mpz_class svX = tc->getInputValue("X");

		// Compute correct value
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
	*/

		OperatorPtr Exp::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
			int wE, wF, k, d, g;
			bool fullInput, IEEEFPMode;
			ui.parseStrictlyPositiveInt(args, "wE", &wE);
			ui.parseStrictlyPositiveInt(args, "wF", &wF);
			ui.parsePositiveInt(args, "k", &k);
			ui.parsePositiveInt(args, "d", &d);
			ui.parseInt(args, "g", &g);
			ui.parseBoolean(args, "IEEEFPMode", &IEEEFPMode);
			ui.parseBoolean(args, "fullInput", &fullInput);
			return new Exp(parentOp, target, wE, wF, k, d, g, fullInput, IEEEFPMode);
		}

	template <>
	const OperatorDescription<Exp> op_descriptor<Exp> {
	    "Exp", // name
	    "A helper operator for the faithful floating-point exponential function.",
	    "ElementaryFunctions",
	    "", // seeAlso
	    "wE(int): exponent size in bits; \
         wF(int): mantissa size in bits;  \
         d(int)=0: degree of the polynomial. 0 choses a sensible default.; \
         k(int)=0: input size to the range reduction table, should be between 5 and 15. 0 choses a sensible default.;\
         g(int)=-1: number of guard bits;\
				 IEEEFPMode(bool)=0: true when the output format is destined to be IEEEFP;\
         fullInput(bool)=0: input a mantissa of wF+wE+g bits (useful mostly for FPPow)",
	    "Parameter d and k control the DSP/RamBlock tradeoff. In both "
	    "cases, a value of 0 choses a sensible default. Parameter g is "
	    "mostly for internal use.<br> For all the details, see <a "
	    "href=\"bib/flopoco.html#DinechinPasca2010-FPT\">this "
	    "article</a>."};
}





