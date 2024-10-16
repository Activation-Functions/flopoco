#ifndef __CarryGenerationCircuit_HPP
#define __CarryGenerationCircuit_HPP
#include <vector>
#include <sstream>
#include <gmp.h>
#include <gmpxx.h>
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco{

	/** The CarryGenerationCircuit class for experimenting with adders.
	 */
	class CarryGenerationCircuit : public Operator
	{
	public:
		/**
		 * The CarryGenerationCircuit constructor
		 * @param[in] target the target device
		 * @param[in] wIn    the width of the inputs and output
		 * @param[in] inputDelays the delays for each input
		 **/
		CarryGenerationCircuit(Target* target, int wIn, map<string, double> inputDelays = emptyDelayMap);
		/*CarryGenerationCircuit(Target* target, int wIn);
		  void cmn(Target* target, int wIn, map<string, double> inputDelays);*/

		/**
		 *  Destructor
		 */
		~CarryGenerationCircuit();

		void outputVHDL(std::ostream& o, std::string name);
		void emulate(TestCase* tc);

		// User-interface stuff
		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
		static void registerFactory();

	protected:
		int wIn_;                         /**< the width for X, Y and R*/
	private:
		map<string, double> inputDelays_; /**< a map between input signal names and their maximum delays */

	};
}
#endif
