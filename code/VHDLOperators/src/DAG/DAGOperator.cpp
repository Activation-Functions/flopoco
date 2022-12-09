/*
  Floating Point Adder for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors:   Bogdan Pasca, Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, 2008-2010.
  All right reserved.

  */

// TODO replace all the cerr / cout with REPORT

/* header of libraries to manipulate multiprecision numbers
   There will be used in the emulate function to manipulate arbitrary large
   entries */
#include "gmp.h"
#include "mpfr.h"

#define DAGOPERATOR_IMPLEM // to prevent the compilation of private stuff in user code 
#include "flopoco/DAGOperator.hpp"
#include "flopoco/utils.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/UserInterface.hpp"
// general c++ library for manipulating streams
#include <iostream>
#include <sstream>
#include <assert.h>
#include <iterator>
#include <map>
#include <string> 
#include <fstream>
#include <vector>
#include "peglib.h"

using namespace std;
//using namespace peg;


namespace flopoco {

string removeSpaces(std::string str){
		str.erase(remove(str.begin(), str.end(), ' '), str.end());
		return str;
}

	// OpInstanceToRetSignalName function for argument parsing for DAG operator e.g FPAdd(FPMult(A,B),B) -> FPAdd(FPMultR,B)
  	string opInstanceToRetSignalName(std::string const& s){
    	std::string::size_type pos = s.find('(');
    	if (pos != std::string::npos){
        	return s.substr(0, pos);
    	}
    	else{
        	return s;
    	}
	}



	void DAGOperator::parse(string infile) {

		//Grammar definitio
		peg::parser DAGparser(R"(
dag                  <- command*
command              <- parameterDeclaration / fileName / operatorDeclaration / InputDeclaration / OutputDeclaration / Assignment / LineComment/ EndOfLine
fileName             <- 'Name' entityName ';'
parameterDeclaration <- 'Param' paramName '='  number ';'
operatorDeclaration  <- 'Operator' instanceName ':' entityName nameValuePair* ';'
nameValuePair        <- name '=' value
InputDeclaration     <-  'Input' signalName ':' typeName '(' value ',' value ')' ';'
OutputDeclaration    <-  'Output' signalName ':' typeName '(' value ',' value ')' ';'

Assignment     <-  signalName '=' instance ';'
instance       <-  instanceName  '('  arg (','  arg)* ')'
arg            <-  instance / signalName

typeName        <- name 
instanceName    <- name 
entityName      <- name
paramName       <- name
signalName      <- name
value           <- string / number / paramvalue
string          <-  < '"' (!'"' .)* '"' >
number          <- < '-'? [0-9]+ >
name            <- < [a-zA-Z] [a-zA-Z0-9-_]* >
paramvalue      <- '$' name
%whitespace     <- [ \t\r\n]*

Endl                      <-  EndOfLine / EndOfFile
EndOfLine                <-  '\r\n' / '\n' / '\r'
EndOfFile                <-  !.
LineComment              <-  ('#' / '//') (!Endl .)* &Endl   


)");

		assert(static_cast<bool>(DAGparser) == true);

		DAGparser["parameterDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto param = removeSpaces(std::any_cast<string>(vs[0]));
			auto val = removeSpaces(any_cast<string>(vs[1]));
			parameters.insert(pair<string, string>("$"+param,val));
			cerr << "PARAM " << param << " " << val << endl;
			return 0;
		};

		DAGparser["fileName"] = [&](const peg::SemanticValues &vs) {
			auto name = std::any_cast<string>(vs[0]);
			fileName = name;
			cout <<  "NAME " << name << endl;
			return 0;
		};
		cerr <<"JITVB0" << endl;

		DAGparser["operatorDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto instanceName = std::any_cast<string>(vs[0]);
			auto entityName = std::any_cast<string>(vs[1]);
			operatorEntity[instanceName] = entityName;
			cout <<   "vs.size = " << vs.size() << endl;
#if 0
			for (int i=0; i<vs.size();i++){			
				cout << i << "  ";
				string name = std::any_cast<string>(vs[i]);
				cout << name << endl;
			}
#endif
			return 0;		
		};

		DAGparser["nameValuePair"] = [&](const peg::SemanticValues &vs) {
			auto name = std::any_cast<string>(vs[0]);
			auto value = std::any_cast<string>(vs[1]);
			cout << "nameValuePair: " << name <<"="<< value <<endl;
			return 0;
		};

		DAGparser["InputDeclaration"] = [&](const peg::SemanticValues &vs) {
			return 0;
		};

		DAGparser["OutputDeclaration"] = [&](const peg::SemanticValues &vs) {
			return 0;
		};

		DAGparser["Assignment"] = [&](const peg::SemanticValues &vs) {
			return 0;
		};

		DAGparser["instance"] = [&](const peg::SemanticValues &vs) {		
			return 0;
		};

		DAGparser["paramName"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["name"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["typeName"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["entityName"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["signalName"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["instanceName"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["value"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["number"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["arg"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };
		DAGparser["topInstance"] = [](const peg::SemanticValues &vs) { return vs.token_to_string(); };

		cerr <<"JITVBn" << endl;
		DAGparser.enable_packrat_parsing(); // Enable packrat parsing.
		cerr <<"JITVB1" << endl;

  	ifstream f;
  	string fileContent;

    // Read file
  	f.open(infile , ios::in);
  	if ( f.is_open() ) {
		  char mychar;
		  while (f) {
			  mychar = f.get();
			  fileContent += mychar;
			}
		}
		else {
			THROWERROR("problem opening " << infile);
		}
		int val;
		DAGparser.parse(fileContent, val);

	}

	
	
DAGOperator::DAGOperator(OperatorPtr parentOp, Target* target, string infile) : Operator(parentOp, target){

	parse(infile);	
	};


	
	void DAGOperator::emulate(TestCase * tc) {
		/* This is a big TODO. 
		 In the general case there is no hope to build this function automatically.
		 What we could have is a sollya-syntaxed description of what the DAG is supposed to compute and test against that, but then issue of accuracy remains.
*/
	}



	OperatorPtr DAGOperator::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
		string infile;
		ui.parseString(args, "infile", &infile);

		return new DAGOperator(parentOp, target, infile);
	}
	
	template <>
	const OperatorDescription<DAGOperator> op_descriptor<DAGOperator> {
	 	  "DAGOperator", // name
			"DAG operator for FloPoCo.", // description, string
			"Miscellaneous", // category, from the list defined in UserInterface.cpp
			"", //seeAlso
			// Now comes the parameter description string.
			"infile(string): name of the file containing the DAG specification",
			// More documentation for the HTML pages. If you want to link to your blog, it is here.
			""
				}; 

}//namespace
