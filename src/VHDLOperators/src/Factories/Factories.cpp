/*
  the FloPoCo command-line interface

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Authors : Florent de Dinechin, Florent.de.Dinechin@ens-lyon.fr
			Bogdan Pasca, Bogdan.Pasca@ens-lyon.org

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA-Lyon
  2008-2014.
  All rights reserved.

*/
#include "flopoco/UserInterface.hpp"

namespace flopoco{
#define FLOPOCO_OPERATOR(op)	\
class op {	\
public:\
	static void registerFactory();\
};
#include "flopoco/Operators.def"
#undef FLOPOCO_OPERATOR
	
void UserInterface::registerFactories()
{
	try {
        #define FLOPOCO_OPERATOR(op) op::registerFactory();
        #include "flopoco/Operators.def"
        #undef FLOPOCO_OPERATOR
		} catch (const std::exception &e) {
			cerr << "Error while registering factories: " << e.what() <<endl;
			exit(EXIT_FAILURE);
		}
	}
}
