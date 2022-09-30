#ifndef GenericLut_H
#define GenericLut_H

#include <string>
#include <vector>

#include "flopoco/Operator.hpp"
#include "flopoco/PrimitiveComponents/BooleanEquation.hpp"
#include "flopoco/utils.hpp"

namespace flopoco {

	// new operator class declaration
    class GenericLut : public Operator {
        std::vector<bool_eq> equations_;
        unsigned int wIn_;
        unsigned int wOut_;
      public:
        GenericLut(Operator* parentOp, Target *target, const std::string &name, const std::vector<bool_eq> &equations );

        GenericLut(Operator* parentOp, Target *target, const std::string &name, const std::map<unsigned int, unsigned int> &groups, const unsigned int &wIn = 0, const unsigned int &wOut = 0 );

        void build();

        void build_select();

		// destructor
        ~GenericLut() {};

			static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args);

			static void registerFactory();
	};


}//namespace

#endif
