/*
  Euclidean division by a small constant

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

*/

// TODOs: remove from d its powers of two .

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>

#include "flopoco/ConstMult/IntConstDiv.hpp"
#include "flopoco/ConstMult/IntConstMultShiftAddPlain.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Tables/TableOperator.hpp"


using namespace std;


namespace flopoco{


	// Divides X_i by D and packs the result in one mpz
	// with the remainder in the lower part 
	vector<mpz_class>  IntConstDiv::euclideanDivTable(int d, int alpha, int rSize) { 		// machine integer arithmetic should be safe for the inputs
		vector<mpz_class>  result;
		for (int x=0; x<(d<<alpha); x++){
			mpz_class xsi = x;
			mpz_class q = xsi/d;
			mpz_class r = xsi%d;
			//cerr << mpz_class( (q<<rSize) + r) << ", " ;
			result.push_back((q<<rSize) + r );
		}
		return result;
	}



	vector<mpz_class>  IntConstDiv::firstLevelCBLKTable( int d, int alpha, int rSize ) {
		vector<mpz_class>  result;
		for (int x=0; x<(1<<alpha); x++) {
			mpz_class r = x % d;
			mpz_class q = x / d;
			result.push_back( (q<<rSize) + r ) ;
		}
		return result;
	}
		
	vector<mpz_class>  IntConstDiv::otherLevelCBLKTable(int level, int d, int alpha, int rSize, int rho ) {
		/* input will be a group of 2^level alpha-bit digits*/
		vector<mpz_class>  result;
		for (int x=0; x<(1<<(2*rSize)); x++) {
			// the input consists of two remainders
			int r0 = x & ((1<<rSize)-1);
			if(r0>=d) r0=0; // This should be a 'don't care'
			int r1 = x >> rSize;
			if(r1>=d) r1=0; // This should be a 'don't care'
			mpz_class y = mpz_class(r0) + (mpz_class(r1) << alpha*((1<<(level-1))) );
			mpz_class r = y % d;
			mpz_class q = y / d;
			result.push_back( (q<<rSize) + r ) ;
		}
		return result;
	}		


		
	int IntConstDiv::quotientSize() {return qSize; };

	int IntConstDiv::remainderSize() {return rSize; };


	// This method mostly replicates the computations done in the constructor
	// l is the number of inputs to the LUT of the target FPGA
	int evaluateLUTCostOfLinearArch(int wIn, int d, int l) {
		int rSize = sizeInBits(d-1);
		int alpha = l-rSize;
		if (alpha<1)	alpha=1;
		int xDigits = wIn/alpha;
		int xPadBits = wIn%alpha;
		if (xPadBits!=0)
			xDigits++;
				
		int tableOutputSize = alpha + rSize;
		int lutsPerOutputBit = 1<<(alpha+rSize-l);
		int cost = tableOutputSize * lutsPerOutputBit * xDigits;
		// cout << "l=" << l << " k=" << alpha << " r=" << rSize << " xDigits=" << xDigits << endl; 
		return cost;
	}



	void IntConstDiv::generateVHDL(){

	}
		

	IntConstDiv::IntConstDiv(OperatorPtr parentOp, Target* target, int wIn_, vector<int> divisors, int alpha_, int architecture_,  bool computeQuotient_, bool computeRemainder_)
		: Operator(parentOp, target),  wIn(wIn_), alpha(alpha_), architecture(architecture_), computeQuotient(computeQuotient_),  computeRemainder(computeRemainder_)
	{
		setCopyrightString("F. de Dinechin (2016)");
		srcFileName="IntConstDiv";
		std::ostringstream o;
		d=1;
		o << "IntConstDiv_";
		if(computeQuotient)
			o << "Q";
		if(computeRemainder)
			o << "R";
		o << "_";
		for(unsigned int i=0; i<divisors.size(); i++) {
			d*=divisors[i];
			o << divisors[i];
			if (i<divisors.size()-1)
				o << "x";
		}
		
		REPORT(LogLevel::DETAIL, "Composite division, d=" << d);

		rSize = sizeInBits(d-1);

		if(alpha==-1){
			if(architecture==0) {
				alpha = getTarget()->lutInputs()-rSize;
				if (alpha<1) {
					alpha=1;
				}
			}
			else {
				alpha = getTarget()->lutInputs();
			}
		}

		qSize = sizeInBits(  ((mpz_class(1)<<wIn)-1)/d  );
		if(!computeQuotient && !computeRemainder) {
			THROWERROR("Neither quotient, neither remainder to compute: better die just now")
		}
		o << "_" << wIn << "_"  << architecture<< "_" << alpha ;
		setNameWithFreqAndUID(o.str());

		addInput("X", wIn);
				

		if(computeQuotient)
			addOutput("Q", qSize);
		if(computeRemainder)
			addOutput("R", rSize);

		int wInCurrent=wIn;
		int currentDivProd=1;
		int overallCostUp=0;
		for(unsigned int i=0; i<divisors.size(); i++) {
			cost=evaluateLUTCostOfLinearArch(wInCurrent, divisors[i], getTarget()->lutInputs());
			REPORT(LogLevel::DETAIL, "Dividing by " << divisors[i] << " for wIn=" << wInCurrent << " should cost about " << cost << " LUTs");
			currentDivProd *= divisors[i];
			wInCurrent = sizeInBits( ((mpz_class(1)<<wIn)-1) / currentDivProd );
			overallCostUp+=cost;
		}
		REPORT(LogLevel::DETAIL, "  Overall cost of composite division: " << overallCostUp << " LUTs");
#if 0
			wInCurrent=wIn;
			currentDivProd=1;
			int overallCostDown=0;
			for(int i=divisors.size()-1; i>=0; i--) {
				cost=evaluateLUTCostOfLinearArch(wInCurrent, divisors[i], getTarget()->lutInputs());
				REPORT(LogLevel::DETAIL, "Dividing by " << divisors[i] << " for wIn=" << wInCurrent << " costs " << cost);
				currentDivProd *= divisors[i];
				wInCurrent = sizeInBits( ((mpz_class(1)<<wIn)-1) / currentDivProd );
				overallCostDown+=cost;
			}
			REPORT(LogLevel::DETAIL, "  Overall cost of composite division, counting down: " << overallCostDown );

			// Now we reverse
			if(overallCostDown<overallCostUp) {
				REPORT(LogLevel::DETAIL, "Reversing the divisor list");
				reverse(divisors.begin(), divisors.end());
			}
#endif


			
			vhdl << tab << declare("Q0",wIn) << " <= X;" << endl;
			wInCurrent=wIn;
			unsigned int i;
			for(i=0; i<divisors.size(); i++) {
				ostringstream params, inportmap,outportmap;
				params << "wIn="<< wInCurrent << " d=" << divisors[i];
				inportmap << "X=>Q"<<i;
				outportmap << "Q=>Q"<<i+1<<",R=>R"<<i+1;
				newInstance("IntConstDiv", join("subDiv",i), params.str(), inportmap.str(), outportmap.str());

				// REPORT(LogLevel::DETAIL, join("subDiv",i) << " " <<  params.str() << "  " << inportmap.str() << "   " << outportmap.str());
				// Slight sub-optimality here, TODO: we can win one bit from time to time
				// wInCurrent = sizeInBits( ((mpz_class(1)<<wIn)-1) /divisors[i]);
				wInCurrent = getSignalByName(join("Q",i+1))->width();
			}
			// Now rebuild the remainder
			// for (3,5,7) R = R1 + 3*(R2+3*R3))
			// R = R1 + divisors[0] *(R2 +divisors[1]*R3) )
			
			// recurrence is  RR2=R3   RR_i-1 = R[i] + divisors[i-1]*RR_i    R=RR_0
			vhdl << tab << declare(join("RR",divisors.size()), getSignalByName(join("R",divisors.size()))->width()) << " <= " << join("R",divisors.size()) << ";" << endl;					
			currentDivProd=divisors[divisors.size()-1];
			for(i=divisors.size()-1; i>=1; i--) {
				//				currentDivProd *= divisors[i];
				// wInCurrent = getSignalByName(join("Q",i+1))->width();
				ostringstream multParams;
				multParams << "wIn=" << sizeInBits(currentDivProd) << " constant=" << divisors[i-1];
				newInstance("IntConstMultShiftAddPlain", join("rMult",i), multParams.str(), "X=>"+join("RR",i+1), "R=>"+join("M",i));
				currentDivProd *= divisors[i-1];
				int sizeRR = sizeInBits(currentDivProd);
				int sizeM  = getSignalByName(join("M",i))->width();
				// int sizePreviousRR =  getSignalByName(join("RR",i+1))->width();
				vhdl << tab << declare(join("RR",i), sizeRR) << " <= ";
				//				if (sizePreviousRR<sizeRR)
				//	vhdl << zg(sizeRR-sizePreviousRR) << "&";
				vhdl << "R" << i << + " + M" << i
						 << (sizeRR<sizeM? range(sizeInBits(currentDivProd)-1,0)  : "")
						 <<  ";" << endl;
			}
			vhdl << tab << "Q <= Q" << divisors.size()<<range(qSize-1,0) << ";" << endl;
			vhdl << tab << "R <= RR1;" << endl;
			

			
	}



	
	IntConstDiv::IntConstDiv(OperatorPtr parentOp, Target* target, int wIn_, int d_, int alpha_, int architecture_,  bool computeQuotient_, bool computeRemainder_)
		: Operator(parentOp, target), d(d_), wIn(wIn_), alpha(alpha_), architecture(architecture_), computeQuotient(computeQuotient_),  computeRemainder(computeRemainder_)
	{
		setCopyrightString("F. de Dinechin (2011, 2016)");
		srcFileName="IntConstDiv";

		if ((d&1)==0) {
			THROWERROR("Divisor is even! Please manage this outside IntConstDiv");
		}

		//		if((architecture==INTCONSTDIV_LINEAR_ARCHITECTURE) || (architecture==INTCONSTDIV_LOGARITHMIC_ARCHITECTURE)) {
		rSize = sizeInBits(d-1);
		
			
		if(alpha==-1){
			if(architecture==0) {
				alpha = getTarget()->lutInputs()-rSize;
				if (alpha<1) {
					REPORT(LogLevel::MESSAGE, "WARNING: This value of d is too large for the LUTs of this FPGA (alpha="<<alpha<<").");
					REPORT(LogLevel::MESSAGE, " Building an architecture nevertheless, but it may be very large and inefficient.");
					alpha=1;
					}
			}
			else {
#if 0 // FOR DEBUG IN HEXA
				alpha=4;
#else
				alpha = getTarget()->lutInputs();
#endif				
			}
		}
			
		qSize = sizeInBits(  ((mpz_class(1)<<wIn)-1)/d  );
 
		
		std::ostringstream o;
		o << "IntConstDiv_";
		if(computeQuotient)
			o << "Q";
		if(computeRemainder)
			o << "R";
		o <<"_"<< d << "_" << wIn << "_"  << architecture<< "_" << alpha ;
		setNameWithFreqAndUID(o.str());

		addInput("X", wIn);
				

		if(computeQuotient)
			addOutput("Q", qSize);
		if(computeRemainder)
			addOutput("R", rSize);


		// First evaluate the cost of the atomic divider
		int cost0=evaluateLUTCostOfLinearArch(wIn, d, getTarget()->lutInputs());		  
		REPORT(LogLevel::DETAIL, "  Estimated cost: " << cost0 );
		
		vector<int> divisors;

		int divofd=3;
		int dd=d;
		while (dd>1){
			while(dd % divofd ==0){
				REPORT(LogLevel::DETAIL, "Found divisor: " << divofd);
				divisors.push_back(divofd);
				dd = dd/divofd;
			}
			divofd +=2;
		}

		
		if(divisors[0]!=d) { // which means that there is more than one
			REPORT(LogLevel::DETAIL, "This constant can be decomposed in smaller factors, consider doing it. I'm building an atomic divider anyway.");
		}



		rho = sizeInBits(  ((mpz_class(1)<<alpha)-1)/d  );

		REPORT(LogLevel::DETAIL, "alpha="<<alpha);
		REPORT(LogLevel::DEBUG, "rSize=" << rSize << " qSize=" << qSize << " rho=" << rho);

		if((d&1)==0)
			REPORT(LogLevel::MESSAGE, "WARNING, d=" << d << " is even, this is suspicious. Might work nevertheless, but surely suboptimal.")


				int xDigits = wIn/alpha;
		int xPadBits = wIn%alpha;
		if (xPadBits!=0)
			xDigits++;
		
		int qDigits = qSize/alpha;
		if ( (qSize%alpha) !=0)
			qDigits++;

		
		REPORT(LogLevel::DETAIL, "Architecture splits the input in xDigits=" << xDigits  <<  " chunks."   );
		REPORT(LogLevel::DEBUG, "  d=" << d << "  wIn=" << wIn << "  alpha=" << alpha << "  rSize=" << rSize <<  "  xDigits=" << xDigits  <<  "  qSize=" << qSize );

		if(architecture==INTCONSTDIV_LINEAR_ARCHITECTURE) {
			//////////////////////////////////////// Linear architecture //////////////////////////////////:

			if(rSize>4) {
				REPORT(LogLevel::MESSAGE, "WARNING: This operator is efficient for small constants. " << d << " is quite large. Attempting anyway.");
			}
			
		vector<mpz_class> tableContent = euclideanDivTable(d, alpha, rSize);
			auto* table = new TableOperator(this, target, tableContent, "", alpha+rSize, alpha+rSize , true);
			table->setNameWithFreqAndUID("EuclideanDivTable_d" + to_string(d) + "_alpha"+ to_string(alpha));
			table->setShared();
			string ri, xi, ini, outi, qi;
			ri = join("r", xDigits);
			vhdl << tab << declare(ri, rSize) << " <= " << zg(rSize, 0) << ";" << endl;

			for (int i=xDigits-1; i>=0; i--) {
				//			cerr << i << endl;
				xi = join("x", i);
				if(i==xDigits-1 && xPadBits!=0) // at the MSB, pad with 0es
					vhdl << tab << declare(xi, alpha, true) << " <= " << zg(alpha-xPadBits, 0) <<  " & X" << range(wIn-1, i*alpha) << ";" << endl;
				else // normal case
					vhdl << tab << declare(xi, alpha, true) << " <= " << "X" << range((i+1)*alpha-1, i*alpha) << ";" << endl;
				ini = join("in", i);
				vhdl << tab << declare(ini, alpha+rSize) << " <= " << ri << " & " << xi << ";" << endl; // This ri is r_{i+1}
				outi = join("out", i);

				newSharedInstance(table, join("table",i), "X=>"+ini, "Y=>"+ outi);

				ri = join("r", i);
				qi = join("q", i);
				vhdl << tab << declare(qi, alpha, true) << " <= " << outi << range(alpha+rSize-1, rSize) << ";" << endl;
				vhdl << tab << declare(ri, rSize) << " <= " << outi << range(rSize-1, 0) << ";" << endl;
			}


			if(computeQuotient) { // build the quotient output
				vhdl << tab << declare("tempQ", xDigits*alpha) << " <= " ;
				for (unsigned int i=xDigits-1; i>=1; i--)
					vhdl << "q" << i << " & ";
				vhdl << "q0 ;" << endl;
				vhdl << tab << "Q <= tempQ" << range(qSize-1, 0)  << ";" << endl;
			}

			if(computeRemainder) { // build the remainder output
				vhdl << tab << "R <= " << ri << ";" << endl; // This ri is r_0
			}
		}






		else if (architecture==INTCONSTDIV_LOGARITHMIC_ARCHITECTURE){
			//////////////////////////////////////// Logarithmic architecture //////////////////////////////////:
			
			// The number of levels is computed out of the number of digits of the _input_
			int levels = sizeInBits(2*xDigits-1); 
			REPORT(LogLevel::DETAIL, "levels=" << levels);
			string ri, xi, ini, outi, qi, qs, r;



			/// First level table
			vector<mpz_class> tableContent = firstLevelCBLKTable(d, alpha, rSize);
			auto* table = new TableOperator(this, target, tableContent, "", alpha, rho+rSize, true);
			table->setShared();
			table->setNameWithFreqAndUID("CBLKTable_l0_d"+ to_string(d) + "_alpha"+ to_string(alpha));
			//			double tableDelay=table->getOutputDelay("Y");
			for (int i=0; i<xDigits; i++) {
				xi = join("x", i);
				if(i==xDigits-1 && xPadBits!=0) // at the MSB, pad with 0es
					vhdl << tab << declare(xi, alpha, true) << " <= " << zg(alpha-xPadBits, 0) <<  " & X" << range(wIn-1, i*alpha) << ";" << endl;
				else // normal case
					vhdl << tab << declare(xi, alpha, true) << " <= " << "X" << range((i+1)*alpha-1, i*alpha) << ";" << endl;
				outi = join("out", i);
				newSharedInstance(table, join("table",i), "X=>"+xi, "Y=>"+ outi);

				ri = join("r_l0_", i);
				qi = join("qs_l0_", i);
				// The qi out of the table are on rho bits, and we want to pad them to alpha bits
				int qiSize;
				if(i<qDigits-1) {
					qiSize = alpha;
					vhdl << tab << declare(qi, qiSize, true) << " <= " << zg(qiSize -rho) << " & (" <<outi << range(rho+rSize-1, rSize) << ");" << endl;
				}
				else if(i==qDigits-1)  {
					qiSize = qSize - (qDigits-1)*alpha;
					REPORT(LogLevel::DETAIL, "-- qsize=" << qSize << " qisize=" << qiSize << "   rho=" << rho);
					if(qiSize>=rho)
						vhdl << tab << declare(qi, qiSize, true) << " <= " << zg(qiSize -rho) << " & (" <<outi << range(rho+rSize-1, rSize) << ");" << endl;
					else
						vhdl << tab << declare(qi, qiSize, true) << " <= " << outi << range(qiSize+rSize-1, rSize) << ";" << endl;
				}
				vhdl << tab << declare(ri, rSize) << " <= " << outi << range(rSize-1, 0) << ";" << endl;
			}

			bool previousLevelOdd = ((xDigits&1)==1);
			// The following levels
			for (int level=1; level<levels; level++){
				int rLevelSize = xDigits/(1<<level); // how many parts with remainder bits we have in this level
				if (xDigits%((1<<level)) !=0 )
					rLevelSize++;
				int qLevelSize = qDigits/(1<<level); // how many sub-quotients we have in this level
				if (qDigits%((1<<level)) !=0 )
					qLevelSize++;
				REPORT(LogLevel::DETAIL, "level=" << level << "  rLevelSize=" << rLevelSize << "  qLevelSize=" << qLevelSize);


				vector<mpz_class> tableContent = otherLevelCBLKTable(level, d, alpha, rSize, rho);
				auto* table = new TableOperator(this, target, tableContent, "",
																 2*rSize, /* wIn*/
																 (1<<(level-1))*alpha+rSize /*wOut*/,
																 true /*logicTable so that it is shared */);
				table->setShared();
				table->setNameWithFreqAndUID("CBLKTable_l" + to_string(level) + "_d"+ to_string(d) + "_alpha"+ to_string(alpha));
				for(int i=0; i<rLevelSize; i++) {
					string tableNumber = "l" + to_string(level) + "_" + to_string(i);
					string tableName = "table_" + tableNumber;
					string in = "in_" + tableNumber;
					string out = "out_" + tableNumber;
					r = "r_"+ tableNumber;
					string q = "q_"+ tableNumber;
					string qsl =  "qs_l" + to_string(level-1) + "_" + to_string(2*i+1);
					string qsr =  "qs_l" + to_string(level-1) + "_" + to_string(2*i);
					qs = "qs_"+ tableNumber; // not declared here because we need it to exit the loop

					if(i==rLevelSize-1 && previousLevelOdd) 
						vhdl << tab << declare(in, 2*rSize) << " <= " << zg(rSize) << " & r_l" << level-1 << "_" << 2*i  << ";"  << endl;
					else
						vhdl << tab << declare(in, 2*rSize) << " <= " << "r_l" << level-1 << "_" << 2*i+1 << " & r_l" << level-1 << "_" << 2*i  << ";"  << endl;

					newSharedInstance(table, "table_"+ tableNumber, "X=>"+in, "Y=>"+ out);

					/////////// The remainder
					vhdl << tab << declare(r, rSize) << " <= " << out << range (rSize-1, 0) << ";"  << endl;

					/////////// The quotient bits
					int subQSize; // The size, in bits, of the part of Q we are building
					if (i<qLevelSize-1) { // The simple chunks where we just assemble a full binary tree
						subQSize = (1<<level)*alpha;
						REPORT(LogLevel::DETAIL, "level=" << level << "  i=" << i << "  subQSize=" << subQSize << "  tableOut=" << table->wOut << " rSize=" << rSize );
						vhdl << tab << declare(q, subQSize) << " <= " << zg(subQSize - (table->wOut-rSize)) << " & " << out << range (table->wOut-1, rSize) << ";"  << endl;
						// TODO simplify the content of the zg above
						vhdl << tab << declare(getTarget()->adderDelay(subQSize), qs, subQSize) << " <= " << q << " + (" <<  qsl << " & " << qsr << ");  -- partial quotient so far"  << endl;
					}
					else if (i==qLevelSize-1){ // because i can reach qLevelSize when rlevelSize=qLevelSize+1, but then we have nothing to do
						// Lefttmost chunk
						subQSize = qSize-(qLevelSize-1)*(1<<level)*alpha;
						REPORT(LogLevel::DETAIL, "level=" << level << "  i=" << i << "  subQSize=" << subQSize << "  tableOut=" << table->wOut << " rSize=" << rSize  << "  (leftmost)");
						
						vhdl << tab << declare(q, subQSize) << " <= " ;
						if(subQSize >= (table->wOut-rSize))
							vhdl << zg(subQSize - (table->wOut-rSize)) << " & " << out << range (table->wOut-1, rSize) << ";"  << endl;
						else
							vhdl << out << range (subQSize+rSize-1, rSize) << ";"  << endl;
						if( (subQSize <= (1<<(level-1))*alpha) ) {
							vhdl << tab << declare(getTarget()->adderDelay(subQSize), qs, subQSize) << " <= " << q << " + " << qsr << ";  -- partial quotient so far"  << endl;
						}
						else {
							vhdl << tab << declare(getTarget()->adderDelay(subQSize), qs, subQSize) << " <= " << q << " + (" <<  qsl << " & " << qsr << ");  -- partial quotient so far"  << endl;
						}

						
					}
				} // for i
				previousLevelOdd = ((rLevelSize&1)==1);
				
			} // for level
			
			if(computeQuotient) { // build the quotient output
				vhdl << tab << "Q <= " << qs  << range(qSize-1, 0) << ";" << endl;
			}
			
			if(computeRemainder) { // build the remainder output
				vhdl << tab << "R <= " << r << ";" << endl;
			}
			
		}



		
		else if (architecture==INTCONSTDIV_RECIPROCAL_ARCHITECTURE){
			//////////////////////////////////////// Reciprocal architecture //////////////////////////////////:
			bool found=false;

			int k = 0;
			while(!found) {
				k ++;
				mpz_class t0 = (mpz_class(1)<<wIn)/d; // This should be the floor
				mpz_class t1 = d*t0 -1;
				mpz_class td = ( (d-1) * (mpz_class(1)<<k)) % d;  // in the article it is (-2^k)%d 
				mpz_class t2 = td*t1 - (mpz_class(1)<<k); 
				// REPORT(LogLevel::DETAIL, "k=" << k << "   td=" << td << "   t2=" << t2 ); 
				found = t2<0;
			}
			int optkp=k;

			k=0; found=false;
			
			while(!found) {
				k ++;
				mpz_class t0 = (mpz_class(1)<<wIn)/d; // This should be the floor
				mpz_class t1 = d*t0 -d +1;
				mpz_class td = (mpz_class(1)<<k) % d;
				mpz_class t2 = td*t1 - (mpz_class(1)<<k); 
				// REPORT(LogLevel::DETAIL, "k=" << k << "   t2=" << t2 ); 
				found = t2<0;
			}
			int optkm=k;
			REPORT(LogLevel::DETAIL, "Found optkp=" << optkp << "   optkm=" << optkm );

			// optkm--; optkp--; // optimality check: if this line is uncommented, it should no longer pass the exhaustive test
			//                   // which is te case. 

			// Attempt to build the optkp
			// We need the ceil of 2^k/d
			mpz_class tmod = (mpz_class(1)<<optkp) % d;
			mpz_class tdiv = (mpz_class(1)<<optkp) / d; // This is the floor
			
			mpz_class ap = (tmod==0? tdiv : tdiv+1);
			IntConstMultShiftAddPlain* multp = new IntConstMultShiftAddPlain(parentOp, target, wIn, ap);
			int costp = multp -> getArea(); 
			
			// Attempt to build the optkm
			// We need the ceil of 2^k/d
			mpz_class am = (mpz_class(1)<<optkm) / d; // This is the floor

			IntConstMultShiftAddPlain* multm = new IntConstMultShiftAddPlain(parentOp, target, wIn, am);
			// TODO this interface is ugly
			int costm = multm -> getArea() + wIn; 

			REPORT(LogLevel::DETAIL, " Cost of optkp version is " << costp << " LUTs" );
			REPORT(LogLevel::DETAIL, " Cost of optkm version is " << costm << " LUTs" );

			// Finally get to VHDL generation
			ostringstream multParams;
			multParams << "wIn=" << wIn << " constant=" << (costp<costm? ap : am);
			newInstance("IntConstMultShiftAddPlain", "recipMult", multParams.str(), "X=>X", "R=>P");
			int pSize=getSignalByName("P")->width();

			if(costp<costm) {
				vhdl << tab << declare("Q1", qSize) << " <= P" << range(pSize-1, pSize-qSize) << ";" << endl;
			}
			else {
				mpz_class b =  ((mpz_class(1)<<optkm) - am*d) * ( (mpz_class(1)<<wIn)/d);
				vhdl << tab << declare("Q0", pSize) << " <= P + \"" << unsignedBinary(b, pSize) << "\";" << endl;

				vhdl << tab << declare("Q1", qSize) << " <= Q0"<< range(pSize-1, pSize-qSize) << ";" << endl;
			}
			if(computeQuotient) {
				vhdl << tab << "Q <= Q1;" <<endl;
			}
			else
				{
					REPORT(LogLevel::DETAIL, "WARNING: this architecture computed the quotient and does not output it; consider using architecture=0, it could be cheaper."); 
				}
			if(computeRemainder) {
				ostringstream multParams;
				multParams << "wIn=" << qSize << " constant=" << d;
				newInstance("IntConstMultShiftAddPlain", "remMult", multParams.str(), "X=>Q1", "R=>QD");
				vhdl << tab << declare("R0",wIn) << " <= X - QD" << range(wIn-1,0) << ";" << endl;
				vhdl << tab << "R <= R0" << range(rSize-1,0) <<";" << endl;
				
			}
		}




		else if (architecture==INTCONSTDIV_TABLE_ADD_ARCHITECTURE){
			/* /////////////////////// Table and add architecture inspired by Arith 2023////////////////////////////:
			The core is a decomposition of X in its digits X_i in radix 2^k:
			X=\sum 2^{ik} X_i
			Then each 2^{ik} X_i  is divided by d:
			\forall i   2^{ik} X_i = d.Q_i + R_i
			This is a tabulation that benefits from the look-up tables of the FPGA: 
			for an FPGA with 6-input LUTs, for k=6 the Q_i and R_i are read from a table 
			that efficiently exploits the LUTs.
			Then X can be rewritten   X = d.(\sum Q_i) + (\sum R_i)
			so we still have to divide by d the smaller value \sum R_i:
			\sum R_i = d.Q'_i + R : this Euclidean division can be tabulated, too
			and finally X = dQ+R  with  Q = (\sum Q_i) + Q'_i 
			The radix-2^k decomposition starts at bit sizeInBits(d): 
			for the lower chunk of sizeInBits(d)-1 bits, the quotient is zero and the remainder is Xi
			*/
			
			string ri, xi, ini, outi, qi;
      vector<int> QiSize;
			int i=0;
			int x0Size=sizeInBits(d)-1;
			REPORT(LogLevel::DETAIL, "chunk " <<0 << " of size " << x0Size);
			xi = join("x", i);
			vhdl << tab << declare(xi, x0Size, true) << " <= " <<  "X" << range(x0Size-1, 0) << ";" << endl;
			// no need to tabulate for this lower chunk
			int chunkLSB=x0Size;
			while (chunkLSB<wIn) {
				i++;
				xi = join("x", i);
				outi = join("out", i);
				qi = join("q", i);
				ri = join("r", i);
				int chunkMSB = min(chunkLSB+alpha,wIn)-1;
				//cerr << "_____________" << i << " chunkLSB=" << chunkLSB  <<  "  chunkMSB=" <<chunkMSB  << endl;
				int chunkSize = chunkMSB-chunkLSB+1;
				REPORT(LogLevel::DETAIL, "chunk " <<i << " of size " << chunkSize);
				
				vhdl << tab << declare(xi, chunkSize, true) << " <= " <<  "X" << range(chunkMSB, chunkLSB) << ";" << endl;

				// building the table content
				vector<mpz_class>  result;
				for (int x=0; x<(1<<chunkSize); x++){
					mpz_class xsi = x;
					xsi = xsi << chunkLSB;
					mpz_class q = xsi/d;
					mpz_class r = xsi%d;
					// cerr << "(" << q << "," << r << ")="  << mpz_class( (q<<rSize) + r) << ", " ;
					result.push_back((q<<rSize) + r );
				}
				int tableOutSize=sizeInBits(result[(1<<chunkSize)-1]);
				TableOperator::newUniqueInstance(this, xi, outi, result, join("DivTable",i), chunkSize, tableOutSize);
				vhdl << tab << declare(qi, tableOutSize-rSize, true) << " <= " <<  outi << range(tableOutSize-1, rSize) << ";" << endl;
				vhdl << tab << declare(ri, rSize, true) << " <= " <<  outi << range(rSize-1, 0) << ";" << endl;
				// prepare for next iteration
				chunkLSB=chunkMSB+1;
			}
			int numberOfChunks = i+1; 

				

			// Now compute \sum R_i + X_0 in a first bit heap, and in parallel \sum Q_i in a second bit heap
			// then divide the first sum by D and update the second sum 

			//REPORT(LogLevel::MESSAGE, "numberOfChunks=" << numberOfChunks);
			int rTildeSize = sizeInBits(numberOfChunks*(d-1));
			BitHeap *rBH = new BitHeap(this, rTildeSize);
			rBH->addSignal("x0"); // x0 is the remainder of the leading bits
			for(i=1; i<numberOfChunks; i++) {
				rBH->addSignal(join("r",i));
			}
			rBH->startCompression();
			vhdl << tab << declare("Rtilde", rTildeSize, true) << " <= " <<  rBH->getSumName() << ";" << endl;

			int tableInSize, tableInShift;
			// Still need to compute the Euclidean division of Rtilde
			if(rTildeSize <= alpha+1) { // a single table will suffice
				tableInSize = rTildeSize;
				tableInShift=0;
				vhdl << tab << declare("RtildeH", tableInSize, true) << " <= " <<  "Rtilde;" << endl;
		}
			else {//split it again into low and high part
				tableInSize = rTildeSize-x0Size;
				tableInShift=x0Size;
				// Again the last bits are already part of the remainder, so for large D it is cheaper to
				// decompose Rtilde into two
				vhdl << tab << declare("RtildeH", tableInSize, true) << " <= " <<  "Rtilde" << range(rTildeSize-1, x0Size) << ";" << endl;
				vhdl << tab << declare("RtildeL", x0Size, true) << " <= " <<  "Rtilde" << range(x0Size-1, 0) << ";" << endl;
			}
				// building the table content
			vector<mpz_class>  result;
			for (int x=0; x<(1<<tableInSize); x++){
				mpz_class xsi = x;
				xsi = xsi << tableInShift;
				mpz_class q = xsi/d;
				mpz_class r = xsi%d;
				// cerr << "d=" << d << "  x=" << x << "  xsi= " << xsi << "  (q,r) = (" << q << "," << r << ")="  << mpz_class( (q<<rSize) + r) << ", " ;
				result.push_back((q<<rSize) + r );
			}
			int tableOutSize=max(rSize,sizeInBits(result[(1<<tableInSize)-1]));
			TableOperator::newUniqueInstance(this, "RtildeH", "LastQR", result, "LastDivTable", tableInSize, tableOutSize);
			vhdl << tab << declare("LastQ", tableOutSize-rSize, true) << " <= " <<  "LastQR" << range(tableOutSize-1, rSize) << ";" << endl;
			vhdl << tab << declare("LastR", rSize, true) << " <= " <<  "LastQR" << range(rSize-1, 0) << ";" << endl;

			if(rTildeSize <= alpha+1) { // we do have the final remainder
				vhdl << tab << declare("FinalFinalR", rSize, true) << "<= LastR;" << endl;			}
			else {
				vhdl << tab << declare("FinalR", rSize+1, true) << " <= " <<  "('0' & LastR) + ('0' & RtildeL);" << endl;
				vhdl << tab << declare("SubR", rSize+1, true) << " <= FinalR-\"" << unsignedBinary(d,rSize+1) << "\";" << endl;
				vhdl << tab << declare("Rupdate") << " <= not SubR" << of(rSize) << ";" << endl;
				
				if(computeRemainder) { // Little to save here if we want to compute only the quotient 
					vhdl << tab << declare("FinalFinalR", rSize, true) << "<= FinalR" << range(rSize-1,0) << " when Rupdate='0' else SubR" << range(rSize-1,0) << ";" << endl;
				}
			}
				// finally, output R
			if(computeRemainder) {
				vhdl << tab << "R <= FinalFinalR;" << endl;
			}


			if(computeQuotient){
				// Finally the last bit heap for quotients
				BitHeap *qBH = new BitHeap(this, qSize );
				for(i=1; i<numberOfChunks; i++) {
					qBH->addSignal(join("q",i));
				}
				qBH->addSignal("LastQ");			
				if(tableInShift>0) {
					qBH->addBit("Rupdate",0);
				}
				
				qBH->startCompression(); 

				// and output Q
				vhdl << tab << "Q <= "<< qBH->getSumName() << ";" << endl;
			}
		}			



		else{
			THROWERROR("arch=" << architecture << " not supported");
		}
		
	}

	IntConstDiv::~IntConstDiv()
	{
	}





	
	void IntConstDiv::emulate(TestCase * tc)
	{
		/* Get I/O values */
		mpz_class X = tc->getInputValue("X");
		/* Compute correct value */
		mpz_class Q = X/d;
		mpz_class R = X%d;
		if(computeQuotient)
			tc->addExpectedOutput("Q", Q);
		if(computeRemainder)
			tc->addExpectedOutput("R", R);
	}



	// if testLevel==-1, run the unit tests, otherwise just compute one single test state  out of testLevel, and return it
	TestList IntConstDiv::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;
    if(testLevel == TestLevel::QUICK) {
			vector<int> constantsToTest={3,241};
			int wIn=24;
			for(auto d: constantsToTest){
				for(int arch=0; arch <4; arch++){
					if(d==241 && arch==1) { // first it doesn't scale, second the VHDL is wrong
						break;
					}
					paramList.push_back(make_pair("wIn", to_string(wIn) ));	
					paramList.push_back(make_pair("d", to_string(d) ));	
					paramList.push_back(make_pair("arch", to_string(arch) ));
					paramList.push_back(make_pair("frequency", to_string(0) )); // because the testbench currently doesn't support unsynchronized outputs 
					testStateList.push_back(paramList);
				}
			}
		}		
    else if(testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests

#if 1
				for(int wIn=8; wIn<34; wIn+=1) 
					{ // test various input widths
						for(int d=3; d<=17; d+=2) 
							{ // test various divisors
								for(int arch=0; arch <4; arch++)
#else // (for debugging)
				for(int wIn=8; wIn<9; wIn+=1) 
					{ // test various input widths
						for(int d=3; d<=3; d+=2) 
							{ // test various divisors
								for(int arch=0; arch <2; arch++)
#endif
									{ // test various architectures // TODO FIXME TO TEST THE LINEAR ARCH, TOO
										paramList.push_back(make_pair("wIn", to_string(wIn) ));	
										paramList.push_back(make_pair("d", to_string(d) ));	
										paramList.push_back(make_pair("arch", to_string(arch) ));
										paramList.push_back(make_pair("frequency", to_string(100) )); // because the testbench currently doesn't support unsynchronized outputs 
										testStateList.push_back(paramList);
										paramList.clear();
									}
							}
					}
			}
		else     
			{
				// finite number of random test computed out of testLevel
			}	

		return testStateList;
	}



	OperatorPtr IntConstDiv::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int wIn, arch, alpha;
		vector<int> divisors;
		bool computeQuotient, computeRemainder;
		ui.parseStrictlyPositiveInt(args, "wIn", &wIn); 
		ui.parseColonSeparatedIntList(args, "d", &divisors);
		ui.parseInt(args, "alpha", &alpha);
		ui.parsePositiveInt(args, "arch", &arch);
		ui.parseBoolean(args, "computeQuotient",  &computeQuotient);
		ui.parseBoolean(args, "computeRemainder", &computeRemainder);
		
		if(divisors.size()==1) {
			return new IntConstDiv(parentOp, target, wIn, divisors[0], alpha, arch, computeQuotient, computeRemainder);
		}
		else { // composite divisor
			return new IntConstDiv(parentOp, target, wIn, divisors, alpha, arch, computeQuotient, computeRemainder);
		}
	}

	template <>
	const OperatorDescription<IntConstDiv> op_descriptor<IntConstDiv> {
	    "IntConstDiv", // name
	    "Integer divider by a small constant.",
	    "ConstMultDiv",
	    "", // seeAlso
	    "wIn(int): input size in bits; \
											 d(intlist): integer to divide by. Either a small integer, or a colon-separated list of small integers, in which case a composite divider by the product is built;  \
											 arch(int)=0: architecture used -- 0 for linear-time, 1 for log-time, 2 for multiply-and-add by the reciprocal, 3 for table-and-addition; \
											 computeQuotient(bool)=true: if true, the architecture outputs the quotient; \
											 computeRemainder(bool)=true: if true, the architecture outputs the remainder; \
											 alpha(int)=-1: Algorithm uses radix 2^alpha. -1 choses a sensible default.",
	    "This operator is described, for arch=0, in <a "
	    "href=\"bib/flopoco.html#dedinechin:2012:ensl-00642145:1\">this "
	    "article</a>, for arch=1, in <a "
	    "href=\"bib/flopoco.html#UgurdagEtAl2016\">this article</a>, for arch=2 in TODO article, and for arch=3 in the TODO-Arith-2023 article."

	};
}
