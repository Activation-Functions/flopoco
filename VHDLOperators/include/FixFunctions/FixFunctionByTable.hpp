#ifndef FixFunctionByTable_HPP
#define FixFunctionByTable_HPP
#include <vector>

#include "Tables/Table.hpp"
#include "FixFunctions/FixFunction.hpp"
#include "Tables/TableOperator.hpp"

namespace flopoco{

	/** The FixFunctionByTable class */
	class FixFunctionByTable : public TableOperator
	{
	public:
		/**
			 The FixFunctionByTable constructor. For the meaning of the parameters, see FixFunction.hpp
		 */

		FixFunctionByTable(OperatorPtr parentOp, Target* target, string func, bool signedIn, int lsbIn, int lsbOut);

		/**
		 * FixFunctionByTable destructor
		 */
		~FixFunctionByTable();

		void emulate(TestCase * tc);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);

		/** Factory register method */
		static void registerFactory();

	protected:

		FixFunction *f;
		unsigned wR;
	};

}

#endif
