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
IOType            <-  'Input' / 'Output'

Assignment     <-  SignalName '<=' Arg ';'
Arg            <-  Instance / SignalName
Instance       <-  InstanceName  '('  Arg (','  Arg)* ')'

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
		// They build a simplified AST
		DAGparser["EntityDeclaration"] = [&](const peg::SemanticValues &vs) {
			string name = any_cast<string>(vs[0]);
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
			auto componentName = any_cast<string>(vs[0]);
			auto EntityName = any_cast<string>(vs[1]);
			componentOperator[componentName] = EntityName;
			REPORT(LogLevel::DEBUG, "OperatorDeclaration: componentName=<" << componentName << ">, EntityName=<"<<EntityName<<">. Found " << vs.size()-2 << " parameter(s)" );
			vector<string> parameterVector; // type copied from Operator's newComponent()
			// string parameters;
			for (size_t i=2; i<vs.size(); i++){
#if 0
				// ask Luc for a modern syntax for the following
				auto valuePair=any_cast<pair<string,string>>(vs[i]);
				auto name=valuePair.first;
				auto value=valuePair.second;
				REPORT(LogLevel::DEBUG, "   name=<" << name << ">, value=<"<<value << ">" );
				// we reconstruct the parameter list as a single string,
				// sounds stupid but it has been parsed and the $ parameters
				// have been replaced with their values
				parameters +=" " + name+"="+value;
#endif
				parameterVector.push_back(any_cast<string>(vs[i]));
			}
			componentParameters[componentName] = parameterVector;
			return 0;		
		};

		DAGparser["NameValuePair"] = [&](const peg::SemanticValues &vs) {
			string name = std::any_cast<string>(vs[0]);
			string value = std::any_cast<string>(vs[1]);
			REPORT(LogLevel::DEBUG, "NameValuePair: <" << name << "=" << value << ">");
			// return pair<string, string>(name,value);
			return name+"="+value;
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
				REPORT(LogLevel::DEBUG, "IODeclaration: <" << name << "> of type <"<< type << ">" );
				if(dagSignalList.find(name)  == dagSignalList.end()) {
					dagSignalList[name] = type;
				}
				else {
					THROWERROR("Signal "<< name << " already declared");
				}
			}
			return 0;		
		};

		/* each instance fills the DAG data structure, 
			 then returns the name of a signal that holds the result */ 
		DAGparser["Instance"] = [&](const peg::SemanticValues &vs) {
			string componentName = any_cast<string>(vs[0]);
			// check we have an Operator for it
			if(componentOperator.find(componentName) == componentOperator.end()) {
				THROWERROR("No Operator for component "<< componentName);
			}
			vector<string> astNode; // it will go to dagNode
			// create a unique instance name
			string uniqueInstanceName = componentName + "_uid"+to_string(getNewUId());	
			// astNode.push_back(uniqueInstanceName); 
			for (size_t i=1; i<vs.size(); i++){
				string name=any_cast<string>(vs[i]);
				if(dagSignalList.find(name) == dagSignalList.end()) {
					THROWERROR("Signal "<< name << " does not seems to be declared");
				}
				astNode.push_back(name);
			}
			instanceComponent[uniqueInstanceName] = componentName;
			dagNode[uniqueInstanceName] = astNode;
			// add an implicit intermediate wire with the same name
			dagSignalList[uniqueInstanceName] = "Wire";
			// A bit of reporting
			string s = "<"+uniqueInstanceName + ">(";
			for (size_t i=1; i<vs.size(); i++){s += "<" +astNode[i-1] + ">";}
			s+=")";
			REPORT(LogLevel::DEBUG, "Instance: " << "<" << uniqueInstanceName << "> (component <"<< componentName << ">, Operator <" << componentOperator[componentName] << ">) <= " << s << ">"  );
			return uniqueInstanceName;
		};

		
		DAGparser["Assignment"] = [&](const peg::SemanticValues &vs) {
			string lhs = any_cast<string>(vs[0]);
			string rhs = any_cast<string>(vs[1]);
			REPORT(LogLevel::DEBUG, "Assignment: <" << lhs << "> <= <" << rhs << ">" );
			if(dagSignalList.find(lhs) == dagSignalList.end()) {
					dagSignalList[lhs] = "Wire";
				}
			else if (dagSignalList[lhs]== "Input"){
				THROWERROR("Attempting to assign to "<< lhs << " which is an input");
			}
			// otherwise we are assigning to an output and it is OK
			if(assignment.count(lhs)  == 0) {
				assignment[lhs] = rhs; 
				// vhdl << tab << lhs << " <= " << rhs << ";" << endl; 
			}
				else {
					THROWERROR("Signal "<< lhs << " already assigned");
				}
		};
		

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
		DAGparser.enable_packrat_parsing(); // not sure what this is
		DAGparser.disable_eoi_check(); // won't work without, don't know why
		ok = DAGparser.parse(fileContent, val);
		if(!ok) {
			THROWERROR("Parse error in " << infile);
		}

	}

	void DAGOperator::checkDAG(){
	}
	
	void DAGOperator::typeInference(){
		// Now we have all it takes to fill the vhdl,
		// but first we need to do a bit of type inference
		// so we create a dummy operator with untyped signals first
		UserInterface::getUserInterface().pushAndClearGlobalOpList();
		Operator dummy(NULL, getTarget());
		for (auto i: dagSignalList) {
			string name=i.first;
			string type=i.second;
			if(type=="Input") {
				dummy.addInput(name);
			}
			if(type=="Output") {
				dummy.addOutput(name);
			}
			if(type=="Wire") {
				dummy.declare(name);
			}
			// maybe wires should be declared later
		}
		// We may only instantiate operators when their inputs are known. 
		// we could start from the output wires and do a recursive depth first build,
		// or wa can just build every node of the DAG as soon as its inputs are known.

		int builtComponentCount=0;
		for (auto i: dagSignalList) {	
			if (i.second=="Input") {
				string name=i.first;
				// Not the actual bit width but it flags them as leaves
				// Actual bit widths will be overwritten by the Operator consuming them
				dagSignalBitwidth[name]=-1;  
			}
		}
		map<string,bool> builtInstance; //initialized to all false
		for (auto i: instanceComponent) {	
			string name=i.first;
			builtInstance[name] = false;
		}
		
		bool progress=true; // detection of infinite loops
		int countOfUntypedSignals = dagSignalList.size();
		do {
			progress=false;
			// build instances for which the signals are inputs or already known
			for (auto i: dagNode) {
				auto uniqueInstanceName=i.first;
				auto args=i.second;
				bool allInputsKnown=true;
				for(size_t i=0; i<args.size(); i++) {
					allInputsKnown &= (dagSignalBitwidth.count(args[i])>0); // boolean and
				}	
				if(allInputsKnown && !builtInstance[uniqueInstanceName]) { // build the operator, the scheduler will not complain
					// Here comes a big restriction: this only works for Operators
					// whose inputs are X,Y,Z... and output is R
					string inPortMap = "X=>" + args[0];
					for(char i=1; i<args.size(); i++) {
						char c='X'+i;
						string pm = (string)(",") + c + "=>" + args[i];
						inPortMap+=pm;
					}
					string returnSignalName="R_"+ uniqueInstanceName;
					string outPortMap = "R=>" + uniqueInstanceName; // signalname==instance name, we'll see
					string componentName=instanceComponent[uniqueInstanceName];
					string opName=componentOperator[componentName];
					auto parameters = componentParameters[componentName];
					string parameterString;
					for(auto i : parameters) {
						parameterString += i + " ";
					}
					REPORT(LogLevel::DEBUG,
								 "First pass with dummy instance " << builtComponentCount
								 <<": "<< uniqueInstanceName << " ("  << componentName << "): " << opName
								 << "  " << parameterString << "  " << inPortMap << " --- " << outPortMap );
					OperatorPtr op= dummy.newInstance(opName, uniqueInstanceName, parameterString, inPortMap, outPortMap);
					builtInstance[uniqueInstanceName]=true;
					builtComponentCount++;
					progress=true;


					// Now we may type the IO signals 
					int sigSize=op->getSignalByName("R")->width();
					dagSignalBitwidth[uniqueInstanceName] = sigSize;
					for(char i=1; i<args.size(); i++) {
						char c='X'+i-1;
						string formal = "";
						formal+=c;
						string actual = args[i];
						sigSize = op->getSignalByName(formal)->width();
						dagSignalBitwidth[actual] = sigSize;
					}
				}
			}
			// We have created instances that could be created, now propagate the result signals through  assignments
			for (auto i: assignment) {
				string lhs = i.first;
				string rhs = i.second;
				if(dagSignalBitwidth.count(rhs)>0 && dagSignalBitwidth.count(lhs)==0) {
					dagSignalBitwidth[lhs] = dagSignalBitwidth[rhs];
					progress=true;
					// cerr << "                " << lhs << " now "  << dagSignalBitwidth[lhs] << endl;
				}
			}
			if (!progress) {
				string message = "Infinite loop in typeInference\n";
				message += "Typed signals so far (";
				message += to_string(dagSignalBitwidth.size());
				message +=  " out of ";
				message +=  to_string(dagSignalList.size()) ;
				message +=  "):\n";
				for(auto i:dagSignalBitwidth){
					message+= "   " + i.first + ":"  + to_string(i.second) + " bits\n" ;
				}						
					THROWERROR(message);			
			}
			// cerr << " ***************** " << builtComponentCount << ":"  << instanceComponent.size() <<endl;
			
		} while(dagSignalBitwidth.size() < dagSignalList.size());

#if 0 // debug info
		for(auto i:dagSignalList){
			cerr << " signal " << i.first << ":"  << i.second <<endl;
		}
		for(auto i:dagSignalBitwidth){
			cerr << " typed signal " << i.first << ":"  << i.second <<endl;
		}
#endif

		UserInterface::getUserInterface().popGlobalOpList();
	}


	
	void DAGOperator::build(){

		// First the IOs, now that we know their size
		for (auto i: dagSignalList) {	
			string name=i.first;
			string iotype=i.second;
			if(iotype=="Input") {
				addInput(name, dagSignalBitwidth[name]);
			};
			if(iotype=="Output") {
				addOutput(name, dagSignalBitwidth[name]);
			};
		}

		// For the schedule to work, we must generate instances from the leaves on.
		// This will be a stupid iteration looking for already produced values
		
		map<string,bool> availableArg; //initialized to all false except inputs
		for (auto i: instanceComponent) {	
			string name=i.first;
			availableArg[name] = false;
		}
		for (auto i: dagSignalList) {	
			string name=i.first;
			string iotype=i.second;
			availableArg[name] = (iotype=="Input");
		}
		
#if 0 // probably useless
		map<string,bool> builtInstance; //initialized to all false
		for (auto i: instanceComponent) {	
			string name=i.first;
			builtInstance[name] = false;
		}
#endif

		// a map to ensure VHDL is output only once 
		map<string,bool> declaredSignal;
		for (auto i: dagSignalList) {	
			string name=i.first;
			string iotype=i.second;
			if (iotype=="Wire") declaredSignal[name] = false;
		}

		bool progress=true; // detection of infinite loops
		while(progress) {
			progress=false;

			// First check if we can build some operator
			for (auto i: dagNode) {
				auto uniqueInstanceName=i.first;
				if (!availableArg[uniqueInstanceName]) {
					auto args=i.second;
					bool allInputsAvailable=true;
					for(size_t i=0; i<args.size(); i++) {
						allInputsAvailable &= availableArg[args[i]]; // boolean and
					}
					if(allInputsAvailable) {
						REPORT(LogLevel::DEBUG,
									 "AIA: instance " << uniqueInstanceName << " can be built");
						availableArg[uniqueInstanceName] = true;
						progress=true;

						// now built it for good
						// VHDL won't accept that an instance name is also a signal name, so we extend istance names
						string actualRHS = (dagNode.count(args[0])==0 ? "":"R_") + args[0];
						string inPortMap = "X=>" + actualRHS;
						for(char i=1; i<args.size(); i++) {
							char c='X'+i;
							actualRHS = (dagNode.count(args[i])==0 ? "":"R_") + args[i];
							string pm = (string)(",") + c + "=>" + actualRHS;
							inPortMap+=pm;
						}
						string returnSignalName="R_"+ uniqueInstanceName;
						string outPortMap = "R=>" + returnSignalName; // signalname==instance name, we'll see if it works
						string componentName=instanceComponent[uniqueInstanceName];
						string opName=componentOperator[componentName];
						auto parameters = componentParameters[componentName];
						string parameterString;
						for(auto i : parameters) {
						parameterString += i + " ";
						}
						REPORT(LogLevel::DEBUG,
									 "actually building instance: "<< uniqueInstanceName
									 << " ("  << componentName << "): " << opName << "  " << parameterString
									 << "  " << inPortMap << " --- " << outPortMap);
						newInstance(opName, uniqueInstanceName, parameterString, inPortMap, outPortMap);
					}
				}
			} // end loop on dagNode
			
			// Next check if we can propagate this information through the assignments
			for (auto i: assignment) {
				string lhs = i.first;
				string rhs = i.second;
				if(availableArg[rhs] && !declaredSignal[lhs]) {
					string actualRHS = (dagNode.count(rhs)==0 ? "":"R_") + rhs;
					if(dagSignalList[lhs]=="Wire") {
						vhdl << tab << declare(lhs, dagSignalBitwidth[lhs]) << " <= " << actualRHS << ";" << endl  ;
					}
					else { // it is an Output
						vhdl << tab << lhs << " <= " << actualRHS << ";" << endl  ;
					}
					availableArg[lhs] = true;
					declaredSignal[lhs] =true;
					progress=true;
				}
			}
			
		} // end while progress
	}
	
	
DAGOperator::DAGOperator(OperatorPtr parentOp, Target* target, string infile) : Operator(parentOp, target){
	parse(infile);
	checkDAG();
	typeInference();
	build();
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
