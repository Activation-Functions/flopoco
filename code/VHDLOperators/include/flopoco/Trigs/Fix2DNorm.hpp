#ifndef Fix2DNorm_HPP
#define Fix2DNorm_HPP

#include <vector>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Trigs/FixAtan2.hpp"
#include "flopoco/utils.hpp"

namespace flopoco{ 
	class Fix2DNorm: public Operator {
	  public:
		/** Constructor:  all unsigned fixed-point number. 
		*/
		Fix2DNorm(OperatorPtr parentOp, Target* target, int msb, int lsb);

		// destructor
		~Fix2DNorm();
		
		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

	private:
	int msb; /**< input/output msb */
	int lsb; /**< input/output lsb */
		
	};
}

#endif
