/*
  Function Table for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon
  
  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,  

  All rights reserved.

  */


#include <iostream>
#include <sstream>
#include <vector>
#include <math.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>

#include <gmpxx.h>
#include "../utils.hpp"

#include "FixFunctionByTable.hpp"

using namespace std;

namespace flopoco{

	FixFunctionByTable::FixFunctionByTable(Target* target, string func, bool signedIn, int lsbIn, int msbOut, int lsbOut, int logicTable, map<string, double> inputDelays):
		Table(target, -lsbIn + (signedIn?1:0), msbOut-lsbOut+1, 0, -1, logicTable){ // This sets wIn
 
		f = new FixFunction(func, signedIn, lsbIn, msbOut, lsbOut);
		ostringstream name;
		srcFileName="FixFunctionByTable";
		
		name<<"FixFunctionByTable_"<<getNewUId(); 
		setName(name.str()); 

		setCopyrightString("Florent de Dinechin (2010-2014)");
		addHeaderComment("-- Evaluator for " +  f-> getDescription() + "\n"); 
					 			
	}

	FixFunctionByTable::~FixFunctionByTable() {
		free(f);
	}
	
	//overloading the method from table
	mpz_class FixFunctionByTable::function(int x){
		mpz_class ru, rd;
		f->eval(mpz_class(x), rd, ru, true);
		return rd;
	}


	void FixFunctionByTable::emulate(TestCase* tc){
		f->emulate(tc, true /* correct rounding */); 
	}
}
