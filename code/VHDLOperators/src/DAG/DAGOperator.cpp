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
	return str; // testing if this function is really useful
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
		//Grammar definition
		// If you edit any of it it is highly recommended to first validate it in the online peglib playground:
		auto grammar = R"(
DAG                  <- EntityDeclaration
EntityDeclaration   <- 'Entity' EntityName '{' Command* '}'
Command              <- ParameterDeclaration  / OperatorDeclaration / IODeclaration / Assignment / Comment
ParameterDeclaration <- 'Parameter' paramName '='  ConstantValue ';'
OperatorDeclaration  <- 'Operator' InstanceName ':' EntityName NameValuePair* ';'
NameValuePair        <- Name '=' Value
IODeclaration     <-  IOType  SignalName ( ','  SignalName)* ';'
IOType            <-  'Input' / 'Output' / 'Wire'

Assignment     <-  SignalName '<=' Instance ';'
Instance       <-  InstanceName  '('  arg (','  arg)* ')'
arg            <-  Instance / SignalName

ConstantValue   <- String / Integer
InstanceName    <- Name
EntityName      <- Name
paramName       <- Name
SignalName      <- Name
Value           <- ConstantValue / ParamValue
String          <-  < '"' (!'"' .)* '"' >
Integer         <- < '-'? [0-9]+ >
Name            <- < [a-zA-Z] [a-zA-Z0-9-_]* >
ParamValue      <- '$' Name

Comment         <- < '#' [^\n]* '\n' > 

%whitespace     <- [ \t\r\n]*
)";

		peg::parser DAGparser;

		DAGparser.set_logger([](size_t line, size_t col, const string& msg, const string &rule) {
			cerr << line << ":" << col << ": " << msg << "\n";
		});
		
		// one wrong character here can lead to random segfaults which are ugly to trace down,
		// so let's keep the grammar validation step.
		auto ok = DAGparser.load_grammar(grammar);
		assert(ok);


		// Tokens just return the string, since this is what we send to the flopoco commandline	
		DAGparser["Name"] = [](const peg::SemanticValues &vs) { 
			return  vs.token_to_string();
		};

		DAGparser["String"] = [](const peg::SemanticValues &vs) { 
			return  vs.token_to_string(); // 
		};

		DAGparser["Integer"] = [](const peg::SemanticValues &vs) { 
			return  vs.token_to_string(); // 
		};

		// Semantic actions for simple grammar rules
		// No need to 
		DAGparser["EntityDeclaration"] = [&](const peg::SemanticValues &vs) {
			string name = any_cast<string>(vs[0]);
			fileName = name;
			REPORT(LogLevel::DEBUG,  "EntityDeclaration: <" << name << ">");;
			setCopyrightString("Florent de Dinechin (2023)");
			setNameWithFreqAndUID(name);

			return 0;
		};
		
		DAGparser["ParameterDeclaration"] = [&](const peg::SemanticValues &vs) {
			string name = any_cast<string>(vs[0]); 
			string value = any_cast<string>(vs[1]); // parameters can be anything, not necessary integersreconstructed paramreconstructed parameter list eter list 
			parameters.insert(pair<string, string>(name,value));
			REPORT(LogLevel::DEBUG,  "ParameterDeclaration: <" << name << "> with value <" << value<< ">");
			return 0;
		};



		DAGparser["ParamValue"] = [&](const peg::SemanticValues &vs) { 
			string paramName = any_cast<string>(vs[0]);
			if(parameters.find(paramName)  == parameters.end()) {
				THROWERROR("Parameter <" << paramName << "> not declared");
			}
			auto value=parameters[paramName];
			REPORT(LogLevel::DEBUG,  "ParamValue : found that the value of $" << paramName << " is " << value);
			return value;
		};



		DAGparser["OperatorDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto instanceName = any_cast<string>(vs[0]);
			auto EntityName = any_cast<string>(vs[1]);
			instanceOperator[instanceName] = EntityName;
			REPORT(LogLevel::DEBUG, "OperatorDeclaration: instanceName=<" << instanceName << ">, EntityName=<"<<EntityName<<">. Found " << vs.size()-2 << " parameter(s)" );
			string parameters;
			for (size_t i=2; i<vs.size(); i++){
				// ask Luc for a modern syntax for the following
				auto valuePair=any_cast<pair<string,string>>(vs[i]);
				auto name=valuePair.first;
				auto value=valuePair.second;
				REPORT(LogLevel::DEBUG, "   name=<" << name << ">, value=<"<<value << ">" );
				// we reconstruct the parameter list as a single string,
				// sounds stupid but it has been parsed and the $ parameters
				// have been replaced with their values
				parameters +=" " + name+"="+value;
			}
			instanceParameters[instanceName] = parameters;
			return 0;		
		};

		DAGparser["NameValuePair"] = [&](const peg::SemanticValues &vs) {
			string name = std::any_cast<string>(vs[0]);
			string value = std::any_cast<string>(vs[1]);
			REPORT(LogLevel::DEBUG, "NameValuePair: <" << name << "=" << value << ">");
			return pair<string, string>(name,value);
		};


		DAGparser["IOType"] = [&](const peg::SemanticValues &vs) {
			string type = vs.token_to_string();
			REPORT(LogLevel::DEBUG, "IOType: <" << type  << ">");
			return type;
		};

		DAGparser["IODeclaration"] = [&](const peg::SemanticValues &vs) {
			string type = any_cast<string>(vs[0]);
			for (size_t i=1; i<vs.size(); i++){
				string name=any_cast<string>(vs[i]);
				dagSignalList[name] = type;
				REPORT(LogLevel::DEBUG, "IODeclaration: <" << name << "> of type <"<< type << ">" );
			}
			return 0;		
		};

		/* each instance returns the name of a signal that holds the result */ 
		DAGparser["Instance"] = [&](const peg::SemanticValues &vs) {
			string instanceName = any_cast<string>(vs[0]);
			REPORT(LogLevel::DEBUG, "Instance: <" << instanceName << ">" );
			vector<string> actualParam;
			for (size_t i=1; i<vs.size(); i++){
				string name=any_cast<string>(vs[i]);
				actualParam.push_back(name);
				REPORT(LogLevel::DEBUG, "   <" << name << ">" );
			}
			string actualRetSignal = "res_"+instanceName + "_"+to_string(getNewUId());	
			REPORT(LogLevel::DEBUG, "  ret <" << actualRetSignal << ">" );
			return actualRetSignal;
		};

		DAGparser["Assignment"] = [&](const peg::SemanticValues &vs) {
			string lhs = any_cast<string>(vs[0]);
			string rhs = any_cast<string>(vs[1]);
			REPORT(LogLevel::DEBUG, "Assignment: <" << lhs << "> <= <" << rhs << ">" );
			// TODO Add the declare
			vhdl << tab << lhs << " <= " << rhs << ";" << endl; 
		};
		
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
		cerr <<"JITVB2" << endl;
		DAGparser.parse(fileContent, val);
		cerr <<"JITVB3 " << val << endl;
		if(val) {
			cerr <<"JITVB4" << endl;
			return ;
		}
		else{
			THROWERROR("Parse error in " << infile);
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
