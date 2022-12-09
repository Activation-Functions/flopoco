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



	void parse(string infile) {


	}

	
	
DAGOperator::DAGOperator(OperatorPtr parentOp, Target* target, string infile) : Operator(parentOp, target){

		//Grammar definitio
		peg::parser DAGparser(R"(
testFile        <- command*
command         <- parameterDeclaration / fileName / operatorDeclaration / InputDeclaration / OutputDeclaration / Assignment / dag / LineComment/ EndOfLine
fileName        <- 'Name' entityName end
parameterDeclaration    <- 'Param' paramName '='  number end
operatorDeclaration          <- 'Operator' entityName (name '=' value)* end 

InputDeclaration           <-  'Input' signalName ':' typeName '(' value ',' value ')' end
OutputDeclaration          <-  'Output' signalName ':' typeName '(' value ',' value ')' end

Assignment     <-  signalName '=' topInstance end
instance        <-  instanceName  '('  arg (','  arg)* ')'
dag             <-  instance end
arg             <-  instance / signalName

topInstance    <- instance
typeName        <- name 
instanceName    <- name 
entityName      <- name
paramName       <- name
signalName      <- name
value           <- name / number 

number          <- < '-'? [0-9]+ >
end             <- ';'
name            <- < [a-zA-Z-$] [a-zA-Z0-9-_]* >
%whitespace     <- [ \t\r\n]*

Endl                      <-  EndOfLine / EndOfFile
EndOfLine                <-  '\r\n' / '\n' / '\r'
EndOfFile                <-  !.
LineComment              <-  ('#' / '//') (!Endl .)* &Endl)");

		assert(static_cast<bool>(DAGparser) == true);

		// When parsing a parameter declaration, insert corresponding value into map
		DAGparser["parameterDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto param = removeSpaces(std::any_cast<string>(vs[0]));
			auto val = removeSpaces(any_cast<string>(vs[1]));
			parameters.insert(pair<string, string>("$"+param,val));
			cerr << "PARAM " << param << " " << val << endl;
			return 0;
		};

		// file name
		string fileName;
		DAGparser["fileName"] = [&](const peg::SemanticValues &vs) {
			auto name = std::any_cast<string>(vs[0]);
			fileName = name;
			cout <<  "NAME " << name << endl;
			return 0;
		};

	
		// operator declaration
		int opNo = 1;
		DAGparser["operatorDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto opName = removeSpaces(std::any_cast<string>(vs[0]));
			auto name = removeSpaces(std::any_cast<string>(vs[1]));
			auto val = removeSpaces(std::any_cast<string>(vs[2]));
			// Check for redefinition of an operator name
			for(int i=1;i<opNo;i++){
				if(val == operatorValues[i]["name"]){
					cerr << "ERROR -> cannot have the same name for two operators, please rename: " << val << endl;
					exit(1);
				}
			}
			//Operator manipulation with map
			operatorValues[opNo].insert(pair<string, string>("type", opName));   // Type of operator
			operatorValues[opNo].insert(pair<string, string>("name", val));      // Name given to operator
			operatorValues[opNo].insert(pair<string, string>("Out1", val+"R"));   // Result wire
			cout <<  "OPERATOR " <<  opName << " " <<  name << " " << val << " ";		
			// Get operator parameters
			int j = 3;int k = 4;int paramNo = 1;
			for (int i=vs.size();i>4;i--){			
				name = std::any_cast<string>(vs[j++]);
				val = removeSpaces(std::any_cast<string>(vs[k++]));
				//checking for $!!!
				if(parameters.find(val) != parameters.end()){val = parameters[val];}
				operatorValues[opNo].insert(pair<string, string>("opParam"+to_string(paramNo), removeSpaces(name)+"="+val));
				cout <<  name << " " << val << " ";
				i--;j++;k++;paramNo++;	
			}
			// Note the amount of arguments used for the operator
			operatorValues[opNo].insert(pair<string, string>("Arguments", to_string(paramNo)));
			cout << endl;
			opNo++;
			cerr<< "Operator declaration OK" <<endl;
			return 0;		
		};

		// Parse input declarations
		int inputs = 1;
		int inputVals[30][2];
		string inputTypes[30];
		DAGparser["InputDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto input = removeSpaces(std::any_cast<string>(vs[0]));
			auto inputType = removeSpaces(std::any_cast<string>(vs[1]));
			auto inputValueOne = removeSpaces(std::any_cast<string>(vs[2]));
			auto inputValueTwo = removeSpaces(std::any_cast<string>(vs[3]));	
			signalList.insert(pair<string, string>("INPUT"+to_string(inputs), input));
			inputTypes[inputs] = inputType;
			if(parameters.find(inputValueOne) != parameters.end()){
				inputVals[inputs][1] = stoi(parameters[inputValueOne]);
			}else{
				inputVals[inputs][1] = stoi(inputValueOne);
			}
			if(parameters.find(inputValueTwo) != parameters.end()){
				inputVals[inputs][2] = stoi(parameters[inputValueTwo]);
			}else{
				inputVals[inputs][2] = stoi(inputValueTwo);
			}
			inputs++;
			cerr<< "Input declaration OK" <<endl;
			return 0;
		};

		// Parse output declaration 
		int outputs = 1;
		int outputVals[30][2];
		string outputTypes[30];
		DAGparser["OutputDeclaration"] = [&](const peg::SemanticValues &vs) {
			auto output = removeSpaces(std::any_cast<string>(vs[0]));
			auto outputType = removeSpaces(std::any_cast<string>(vs[1]));
			auto outputValueOne = removeSpaces(std::any_cast<string>(vs[2]));
			auto outputValueTwo = removeSpaces(std::any_cast<string>(vs[3]));
			signalList.insert(pair<string, string>("OUTPUT"+to_string(outputs), output));		
			inputTypes[outputs] = outputType;
			if(parameters.find(outputValueOne) != parameters.end()){
				outputVals[outputs][1] = stoi(parameters[outputValueOne]);
			}else{
				outputVals[outputs][1] = stoi(outputValueOne);
			}
			if(parameters.find(outputValueTwo) != parameters.end()){
				outputVals[outputs][2] = stoi(parameters[outputValueTwo]);
			}else{
				outputVals[outputs][2] = stoi(outputValueTwo);
			}
			outputs++;
			cerr<< "Output declaration OK" <<endl;
			return 0;

		};

		// parsing assignment (LHS=RHS) : need to find the output signal of the RHS and associate it to the LHS
		DAGparser["Assignment"] = [&](const peg::SemanticValues &vs) {
			auto savedSignal = removeSpaces(std::any_cast<string>(vs[0]));
			auto args = opInstanceToRetSignalName(removeSpaces(std::any_cast<string>(vs[1])))+"R";		
			//
			bool notFound=true;
			for(int i=0;i<opNo;i++){
				if(operatorValues[i]["Out1"] == args){
					signalList.insert(pair<string, string>(savedSignal, removeSpaces(args)));
					notFound=false;
				}
			}
			if(notFound) {
				THROWERROR("ERROR 17");
			}
		
			return 0;
			
		};

	
		// Parsing an  instance order info and arguments
		int instanceOrder = 1;
		DAGparser["instance"] = [&](const peg::SemanticValues &vs) {		
			int operatorNum;
			string instNum = to_string(instanceOrder); 
		
			auto name = removeSpaces(std::any_cast<string>(vs[0]));
			auto arg = removeSpaces(std::any_cast<string>(vs[1]));

			// Find which operator is being called
			for(int i=1;i<opNo;i++){
				if(name == operatorValues[i]["name"]){
					operatorNum = i;   
				}
			}

			// Checking if an operation is saved
			if(signalList.find(arg) != signalList.end()){
				arg = signalList[arg];
			}
			bool found = false;
			int i = 1;
			while (i<inputs && found == false){
				if(signalList["INPUT"+to_string(i)] == arg){
					arg += "_internal";
					found = true;
				}
				i++;
			}
			// Make sure that if operator exists and has been generated, inputs are the same
			if(operatorValues[operatorNum].find("In1") != operatorValues[operatorNum].end() && operatorValues[operatorNum]["In1"] != arg){
				cerr << "ERROR -> cannot use operator " << operatorValues[operatorNum]["name"] << " twice with different arguments !!!" << endl;
				exit(1);
			}
			// Insert first argument
			operatorValues[operatorNum].insert(pair<string, string>("In1", arg));
			found = false;
			// Get arguments for operator
			int j = 2;
			while(j < vs.size()){
				arg = removeSpaces(std::any_cast<string>(vs[j]));
		
				// Checking if an operation is saved
				if(signalList.find(arg) != signalList.end()){
					arg = signalList[arg];
				}
				int k = 1;
				while (k<inputs && found == false){
					if(signalList["INPUT"+to_string(k)] == arg){
						arg += "_internal";
						found = true;
					}
					k++;
				}
				// Make sure that if operator exists and has been generated, inputs are the same
				if(operatorValues[operatorNum].find("In"+to_string(j)) != operatorValues[operatorNum].end() && operatorValues[operatorNum]["In"+to_string(j)] != arg){
					cerr << "ERROR -> cannot use operator " << operatorValues[operatorNum]["name"] << " twice with different arguments !!!" << endl;
					exit(1);
				}
				// Insert next arguments
				operatorValues[operatorNum].insert(pair<string, string>("In"+to_string(j), arg));
				found = false;
				j++;
			}
			// Check if the operator has already been generated, increment the instance order
			if(operatorValues[operatorNum].find("order") == operatorValues[operatorNum].end()){
				operatorValues[operatorNum].insert(pair<string, string>("order", instNum));
				instanceOrder++;
			}
			return 0;
		};

		// Return a string for use in each command
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


		// (4) Parse
		DAGparser.enable_packrat_parsing(); // Enable packrat parsing.

  	ifstream f;
  	string arg;
	  auto expr = arg;

    // Read file
  	f.open(infile , ios::out);
  	if ( f.is_open() ) {
		  char mychar;
		  while (f) {
			  mychar = f.get();
			  expr += mychar;
	  	}
  	}

		int val;
		DAGparser.parse(expr, val);
		//	parse(infile);
	
  //Find out which operators are used together and match wiring
  int sigWireNo = 1; string newInput;
  for(int i=1;i<opNo;i++){
    for(int j=1;j<opNo;j++){

      //OpInstanceToRetSignalName inputs and replace them if operator name is found, with the output wire of the found operator
      if(opInstanceToRetSignalName(operatorValues[i]["In1"]) == operatorValues[j]["name"]){
          operatorValues[i].erase("In1");
          operatorValues[i].insert(pair<string, string>("In1", operatorValues[j]["Out1"]));
      }
      if(opInstanceToRetSignalName(operatorValues[i]["In2"]) == operatorValues[j]["name"]){
          operatorValues[i].erase("In2");
          operatorValues[i].insert(pair<string, string>("In2", operatorValues[j]["Out1"]));
      }
      if(opInstanceToRetSignalName(operatorValues[i]["In3"]) == operatorValues[j]["name"]){
          operatorValues[i].erase("In3");
          operatorValues[i].insert(pair<string, string>("In3", operatorValues[j]["Out1"]));
      }
    }

    //Check if any brackets exist, meaning that a spelling mistake exists
    if(operatorValues[i]["In2"].find('(') != string::npos || operatorValues[i]["In1"].find('(') != string::npos || operatorValues[i]["In3"].find('(') != string::npos){
      cerr << "ERROR -> incorrect spelling or parentheses mistake, arguments in " << operatorValues[i]["name"] << " are misspelled !" <<endl;
		  exit(1);
    }

 }


      //Print each map, for debugging
  map<string, string>::iterator itr;
    cout << "\n***PARAMETER MAP***\n\n";
    cout << "KEY\tELEMENT\n";
    for (itr = parameters.begin(); itr != parameters.end(); ++itr) {
        cout << itr->first << '\t' << itr->second << '\n';
    }

  map<string, string>::iterator itrSig;
    cout << "\n***SIGNAL MAP***\n\n";
    cout << "KEY\tELEMENT\n";
    for (itrSig = signalList.begin(); itrSig != signalList.end(); ++itrSig) {
        cout << itrSig->first << '\t' << itrSig->second << '\n';
    }

  for(int i=1;i<opNo;i++){
      map<string, string>::iterator itrVal;
      cout << "\n***OPERATOR " << i <<  " MAP***\n\n";
      cout << "KEY\tELEMENT\n";
      for (itrVal = operatorValues[i].begin(); itrVal != operatorValues[i].end(); ++itrVal) {
          cout << itrVal->first << '\t' << itrVal->second << '\n';
      }
  }

  cout << endl;


    // Count the amount of parameters
    int paramCounter = 0;
    for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
        paramCounter++;
    }

		// definition of the name of the operator
		ostringstream name;
		name << fileName;
		setName(name.str()); // See also setNameWithFrequencyAndUID()

		// Copyright 
		setCopyrightString("Luke Newman, Florent de Dinechin - INSA Lyon (2022)");

		addFullComment("Start of vhdl generation"); // this will be a large, centered comment in the VHDL

		// declaring inputs
    for(int i=1;i<inputs;i++){
		  addFPInput (signalList["INPUT"+to_string(i)] , inputVals[i][1], inputVals[i][2]);
      declareFloatingPoint((signalList["INPUT"+to_string(i)]+"_internal"), inputVals[i][1], inputVals[i][2]);
      vhdl << tab << signalList["INPUT"+to_string(i)]+"_internal<=" << signalList["INPUT"+to_string(i)]+";"<< endl;
    }

    // declaring outputs
    for(int i=1;i<outputs;i++){
		  addFPOutput (signalList["OUTPUT"+to_string(i)] , outputVals[i][1], outputVals[i][2]);
    }

		// basic message
		//REPORT(LogLevel::INFO,"Declaration of DAGOperator \n");

		// more detailed message
		// REPORT(LogLevel::DETAILED, "this operator has received " << paramCounter <<  " parameters ");
  
		// debug message for developer
		// REPORT(LogLevel::DEBUG,"debug of DAGOperator");

    int nextInSequence = 1;

    //Loop for instances
		for(int j=1;j<=opNo;j++){
      for(int i=1;i<opNo;i++){
        if(stoi(operatorValues[i]["order"]) == nextInSequence){

        //Initialisers for instance creation
        string operatorType = operatorValues[i]["type"];
				auto instanceOpFactory = FactoryRegistry::getFactoryRegistry().getFactoryByName(operatorType);
				
				//        OperatorFactoryPtr instanceOpFactory = UserInterface::getFactoryByName(operatorType);
        vector<string> parametersVector;
        OperatorPtr instance = nullptr;

        //Set parameters
        parametersVector.push_back(operatorType);
        string paramStr;

        //Insert parameters into vector and creating the parameters for instance creation
        for(int x=1; x<stoi(operatorValues[i]["Arguments"]); x++){
          parametersVector.push_back(removeSpaces(operatorValues[i]["opParam"+to_string(x)]));
          if(!paramStr.empty()){paramStr += " ";}
          paramStr += operatorValues[i]["opParam"+to_string(x)];
        }

        //Create instance
        instance = instanceOpFactory->parseArguments(this, target, parametersVector,UserInterface::getUserInterface());

        int InWire = 1; int OutWire = 1; int newWire;
        string inputWiring, outputWiring;

        //Detect and create I/O wires
        for(auto iter : *(instance->getIOList())){
            if(iter->type() == Signal::in)	{
              if(!inputWiring.empty()){inputWiring += ",";}
              inputWiring += iter->getName()+"=>"+operatorValues[i]["In"+to_string(InWire)];
              InWire++;
            }
            if(iter->type() == Signal::out)	{
              if(!outputWiring.empty()){outputWiring += ",";}
                if(OutWire == 1){
                  outputWiring += iter->getName()+"=>"+operatorValues[i]["Out1"];
                }else{
                  newWire = OutWire - 1;
                  operatorValues[i].insert(pair<string, string>("Out"+to_string(OutWire), operatorValues[i]["Out"+to_string(newWire)]+"R"));              
                  outputWiring += iter->getName()+"=>"+operatorValues[i]["Out"+to_string(OutWire)];
                }
                OutWire++;
            }

          // Get bit size of operator and store its value
          operatorValues[i].insert(pair<string, string>("Width", to_string(iter->width())));

		    }
        
				//        REPORT(LogLevel::INFO, " \nNEW INSTANCE BEGINNING " << endl << "Parameters: " <<paramStr << " Input: " << inputWiring << " Output: " << outputWiring << endl);
        
        //starting the new instance
        paramStr += " name=" + operatorValues[i]["name"];
		    newInstance(operatorType, operatorValues[i]["name"], paramStr, inputWiring, outputWiring);

        // Indicate next operator is ready to be generated
        nextInSequence++;

        //Assign the result of the final operator to the output wire
        int outputIter = 1;
        if((instanceOrder-1) == stoi(operatorValues[i]["order"])){
          while(signalList.find("OUTPUT"+to_string(outputIter)) != signalList.end()){
            vhdl << tab << signalList["OUTPUT"+to_string(outputIter)]+"<="+operatorValues[i]["Out"+to_string(outputIter)]+";"  << endl;
            outputIter++;
          }
        }

        }
      }
		}

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
