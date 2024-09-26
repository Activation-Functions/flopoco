#pragma once

#include "flopoco/Operator.hpp"
#include "flopoco/Tables/DifferentialCompression.hpp"
#include "flopoco/Tables/TableCostModel.hpp"

namespace flopoco {
	class DifferentialCompressionHelper {
	public:

		/**  Computes the parameters of the addition, then inserts the corresponding VHDL in Operator op. */
		void insertAdditionVHDL(OperatorPtr op, DifferentialCompression t, string actualOutputName, string subsamplingOutName, string diffOutputName);

		/** This is a drop-in  replacement for Table::newUniqueInstance.
			It will instantiate two tables and an adder, therefore will be suboptimal if the table output goes to a bit heap
		 * @param[in] op            The Operator that will be the parent of this Table (usually "this")
		 * @param[in] actualInput   The actual input name
		 * @param[in] actualOutput  The actual input name
		 * @param[in] values        The vector of mpz_class values to be passed to the Table constructor
		 returns the report
		 */
	string newUniqueInstance(OperatorPtr op,
														string actualInput, string actualOutput,
														vector<mpz_class> values, string name,
														int wIn = -1, int wOut = -1,
														int logicTable=0);
	};
}