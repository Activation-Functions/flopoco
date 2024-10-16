#ifndef INTADDERS_HPP
#define INTADDERS_HPP
#include <sstream>
#include <vector>
#include <gmp.h>
#include <gmpxx.h>

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco {

	/** The IntAdder class for pipelined integer adders.
	 */
	class IntAdder : public Operator {
	public:

		/** The types of integer addition */
		typedef enum {
			Add, /**< plain adder */
			Sub, /**< subtracter */
			AddSub, /**< X+Y, X-Y */
			SubSub, /**< X-Y, Y-X */
		} Type;
		

		/**
		 * The IntAdder constructor
		 * @param[in] parentOp         the parent operator of this component
		 * @param[in] target           the target device
		 * @param[in] wIn              the width of the inputs and output
		 **/
		IntAdder ( OperatorPtr parentOp, Target* target, int wIn);

		/**
		 *  Destructor
		 */
		~IntAdder();

		/**
		 * The emulate function.
		 * @param[in] tc               a list of test-cases
		 */
		void emulate ( TestCase* tc );

		/**
		 * get the maximum adder size for a given target period
		 */
		static int getMaxAdderSizeForPeriod(Target* target, double TargetPeriod);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

	protected:
		int wIn;                                    /**< the width for X, Y and R*/
	private:
		vector<Operator*> addImplementationList;     /**< this list will be populated with possible adder architectures*/
		int selectedVersion;                         /**< the selected version from the addImplementationList */
	};

}
#endif
