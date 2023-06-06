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

	/* Old grammar


	 */

	/* remmove me? 

dag                  <-  command*
command              <- fileName / parameterDeclaration / operatorDeclaration / ioDeclaration / assignment / comment / endOfLine
fileName             <- 'EntityName' name ';'
parameterDeclaration <- 'Param' name '='  number ';'
operatorDeclaration  <- 'Operator' instanceName ':' entityName nameValuePair* ';'
nameValuePair        <- name '=' value
ioDeclaration     <-  ioDir  name ':' name '(' value ',' value ')' ';'
ioDir                <-  'Input' / 'Output'

assignment     <-  name '=' instance ';'
instance       <-  name  '('  arg (','  arg)* ')'
arg            <-  instance / name

instanceName   <- name
entityName   <- name

value           <- string / number / paramValue
string          <-  < '"' (!'"' .)* '"' >
number          <- < '-'? [0-9]+ >
name            <- < [a-zA-Z] [a-zA-Z0-9-_]* >
paramValue      <- '$' name

%whitespace     <- [ \t\n]*
endl                      <-  endOfLine / endOfFile
endOfLine                <-  '\r\n' / '\n' / '\r'
endOfFile                <-  !.
comment              <-  ('#' / '//') (!endl .)* &endl   

*/

	
	void DAGOperator::parse(string infile) {

		//Grammar definitio
		peg::parser DAGparser(R"(
dag                  <- command*
command              <- entityDeclaration / parameterDeclaration / operatorDeclaration / ioDeclaration / assignment / commentLine / EndOfLine
entityDeclaration             <- 'EntityName' entityName ';'
parameterDeclaration <- 'Parameter' name '='  value ';'
operatorDeclaration  <- 'Operator' instanceName ':' entityName nameValuePair* ';'
nameValuePair        <- name '=' value
ioDeclaration     <-  ioDir signalName ':' typeName '(' value ',' value ')' ';'
ioDir    <-  'Input' | 'Output'

assignment     <-  signalName '=' instance ';'
instance       <-  instanceName  '('  arg (','  arg)* ')'
arg            <-  instance / signalName

typeName        <- name 
instanceName    <- name 
entityName      <- name
signalName      <- name
value           <- string / number / paramValue
string          <-  < '"' (!'"' .)* '"' >
number          <- < '-'? [0-9]+ >
name            <- < [a-zA-Z] [a-zA-Z0-9-_]* >
paramValue      <- '$' name

%whitespace     <- [ \t\n]*
Endl                      <-  EndOfLine / EndOfFile
EndOfLine                <-  '\r\n' / '\n' / '\r'
EndOfFile                <-  !.
commentLine              <-  ('#' / '//') (!Endl .)* &Endl   

    )");

		assert(static_cast<bool>(DAGparser) == true);

		DAGparser.set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
			cerr << line << ":" << col << ": " << msg << "\n";
		});


		
		DAGparser["entityDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto name = any_cast<string>(vs[0]);
			fileName = name;
			REPORT(LogLevel::DEBUG,  "entityDeclaration :" << name);;
			return 0;
		};

		DAGparser["EntityName"] = [&](const peg::SemanticValues &vs) { 
			return  any_cast<string>(vs[0]);
		};
		
		DAGparser["name"] = [&](const peg::SemanticValues &vs) { 
			return  removeSpaces(vs.token_to_string());
		};

		DAGparser["value"] = [&](const peg::SemanticValues &vs) { 
			return  vs.token_to_string();
		};

		DAGparser["paramValue"] = [&](const peg::SemanticValues &vs) { 
			string paramName = any_cast<string>(vs[0]);
			if(parameters.find(paramName)  == parameters.end()) {
				THROWERROR("Parameter" << paramName << " not declared");
			}
			auto value=parameters[paramName];
			REPORT(LogLevel::DEBUG,  "paramValue : found that the value of $" << paramName << " is " << value);
			return value;
		};

		
		DAGparser["parameterDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto name = any_cast<string>(vs[0]);
			auto value = any_cast<string>(vs[1]);
			parameters.insert(pair<string, string>(name,value));
			REPORT(LogLevel::DEBUG,  "parameterDeclaration: " << name << " with value " << value);;
			return 0;
		};
		

		DAGparser["operatorDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto instanceName = any_cast<string>(vs[0]);
			auto entityName = any_cast<string>(vs[1]);
			operatorEntity[instanceName] = entityName;
			REPORT(LogLevel::DEBUG, "operatorDeclaration: instanceName=" << instanceName << ", entityName="<<entityName<<". Found " << vs.size()-2 << " parameter(s)" );
			for (size_t i=2; i<vs.size(); i++){
				// ask Luc for a modern syntax for the following
				auto valuePair=any_cast<pair<string,string>>(vs[i]);
				auto name=valuePair.first;
				auto value=valuePair.second;
				REPORT(LogLevel::DEBUG, "   name=" << name << ", value="<<value );
			}
			return 0;		
		};

		DAGparser["nameValuePair"] = [&](const peg::SemanticValues &vs) {
			auto name = std::any_cast<string>(vs[0]);
			auto value = std::any_cast<string>(vs[1]);
			REPORT(LogLevel::DEBUG, "parsing nameValuePair:  " << name << "=" << value);
			return pair<string, string>(name,value);
		};


		
#if 0

		DAGparser["ioDeclaration"] = [&](const peg::SemanticValues &vs) {
						cerr << this << endl;
			return 0;
		};

		DAGparser["ioDir"] = [&](const peg::SemanticValues &vs) {
			return 0;
		};

		DAGparser["assignment"] = [&](const peg::SemanticValues &vs) {
			return 0;
		};

		DAGparser["instance"] = [&](const peg::SemanticValues &vs) {		
			return 0;
		};

		DAGparser["arg"] = [&](const peg::SemanticValues &vs) {		
			return 0;
		};

		// I don't know why Peglib doesnt remove the whitespaces in this case
		DAGparser["paramName"] = [&](const peg::SemanticValues &vs) { return  removeSpaces(vs.token_to_string()); };

		DAGparser["paramValue"] = [&](const peg::SemanticValues &vs) {
			string paramName=removeSpaces(vs.token_to_string());
			auto value=parameters.find(paramName);
			if(value  != parameters.end()) {
				return value;
			}
			else {
				cerr << ("Parameter" << paramName << " not declared");
				return(1);
			}
			return paramName;
		};
		


#endif

		
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
		if(DAGparser.parse(fileContent, val))
			return ;

		else{
			THROWERROR("Parse error");
		}
		
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
