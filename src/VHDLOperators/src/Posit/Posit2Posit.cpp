#include <iostream>
#include <sstream>

#include "gmp.h"
#include "mpfr.h"

#include "flopoco/Posit/Posit2Posit.hpp"
#include "flopoco/Conversions/Posit2PIF.hpp"
#include "flopoco/Conversions/PIF2Posit.hpp"

using namespace std;
namespace flopoco {




  Posit2Posit::Posit2Posit(Target* target, Operator* parentOp, int widthI, int wES) : Operator(parentOp, target), widthI_(widthI), wES_(wES) {

		srcFileName="Posit2Posit";

		// definition of the name of the operator
		ostringstream name;
		name << "Posit2Posit_" << widthI << "_" << wES;
		setNameWithFreqAndUID(name.str());
		// Copyright 
    setCopyrightString("Oregane Desrentes 2019");

		
		// declaring inputs
		addInput ("I" , widthI);
		// declaring output
		addOutput("O" , widthI);

		addFullComment("Start of vhdl generation"); 

		ostringstream param, inmap, outmap;
  
		param << "width=" << widthI;
		param << " wES=" << wES; 

		inmap << "I=>I";

		outmap << "O=>PIF";

		newInstance("Posit2PIF", "c1", param.str(), inmap.str(), outmap.str());

		ostringstream param2, inmap2, outmap2;
  
		param2 << "width=" << widthI;
		param2 << " wES=" << wES; 

		inmap2 << "I=>PIF";

		outmap2 << "O=>O";

		newInstance("PIF2Posit", "c2", param2.str(), inmap2.str(), outmap2.str());

		addFullComment("End of vhdl generation"); 
	};

	
	void Posit2Posit::emulate(TestCase * tc) {
		tc->addExpectedOutput("O", tc->getInputValue("I"));
	}


	void Posit2Posit::buildStandardTestCases(TestCaseList * tcl) {
		// please fill me with regression tests or corner case tests!
	}




	OperatorPtr Posit2Posit::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		int width, wES;
		ui.parseInt(args, "width", &width); 
		ui.parseInt(args, "wES", &wES); 
		return new Posit2Posit(target, parentOp, width, wES);
	}

	template <>
	OperatorFactory op_factory<Posit2Posit>(){return factoryBuilder<Posit2Posit>({
	    "Posit2Posit",
	    "This should do nothing",
	    "Conversions",
	    "",
	    "width(int): The size of the posit; \
                                   wES(int): The width of the exponent",
	    ""});}

}//namespace
