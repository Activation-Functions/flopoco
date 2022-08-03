#ifndef TABLEOPERATOR_HPP
#define TABLEOPERATOR_HPP
#include "Operator.hpp"
#include "Tables/Table.hpp"
namespace flopoco
{

	/**
	 A basic hardware look-up table for FloPoCo.

		 If the input to your table are negative, etc, or if you want to
		 define errors, or... then derive a class from this one.

		 To derive a class with a user interface from Table, see
	 FixFunctions/FixFunctionByTable To use a Table as a sub-component of a
	 larger operator, see FixFunctions/FixFunctionByPiecewisePoly

		 A Table is, so far, always combinatorial. It does increase the
	 critical path

		 On logic tables versus blockRam tables:
		 This has unfortunately to be managed twice,
		   firstly by passing the proper bool value to the logicTable
	 argument of the constructor and secondly by calling useSoftRAM() or
	 useHardRAM() on each instance to set the synthesis attributes.

	*/
	class TableOperator : public Operator
	{

	public:
		/**
			     * The TableOperator constructor
			     It is an exception in FloPoCo as there is no
		   corresponding user interface for it, because passing the
		   vector of content on the command line would be a pain. The
		   proper way to instanciate this component is with
		   newUniqueInstance(), or with one of the subclasses of Table
		   which provide a user interface, such as FixFunctionByTable
			     * @param[in] values 		the values used to
		   fill the table. Each value is a bit vector given as positive
		   mpz_class.
			     * @param[in] wIn    		the width of the input
		   in bits (optional, may be deduced from values)
		     * @param[in] parentOp 	the parent operator in the
		   component hierarchy
		     * @param[in] target 		the target device
		     * @param[in] name
		     * @param[in] logicTable 1 if the table is intended to be
		   implemented as logic; It is then shared
		     *                          -1 if it is intended to be
		   implemented as embedded RAM
			     * @param[in] minIn			minimal input
		   value, to which value[0] will be mapped (default 0)
			     * @param[in] maxIn			maximal input
		   value (default: values.size()-1)
			     */
		TableOperator(OperatorPtr parentOp, Target *target,
			      vector<mpz_class> _values, string name = "",
			      int _wIn = -1, int _wOut = -1,
			      int _logicTable = 0, int _minIn = -1,
			      int _maxIn = -1);

		TableOperator(OperatorPtr parentOp, Target *target);

		virtual ~TableOperator(){};

		/** actual VHDL generation */
		void generateVHDL();

		/** Table has no factory because passing the values vector would
		 * be a pain. This replaces it.
		 * @param[in] op            The Operator that will be the parent
		 * of this Table (usually "this")
		 * @param[in] actualInput   The actual input name
		 * @param[in] actualOutput  The actual input name
		 * @param[in] values        The vector of mpz_class values to be
		 * passed to the Table constructor
		 */
		static OperatorPtr
		newUniqueInstance(OperatorPtr op, string actualInput,
				  string actualOutput, vector<mpz_class> values,
				  string name, int wIn = -1, int wOut = -1,
				  int logicTable = 0);

		/** A function that returns an estimation of the size of the
		 * table in LUTs. Your mileage may vary thanks to boolean
		 * optimization */
		int size_in_LUTs() const;

		/** get one element of the table */
		mpz_class val(int x);

	protected:
		/** A function that does the actual constructor work, so that it
		 * can be called from operators that overload Table.  See
		 * FixFunctionByTable for an example */
		void init(vector<mpz_class> & values, string _name = "", int _wIn = -1, int _wOut = -1, int _logicTable = 0, int _minIn = -1, int _maxIn = -1);

        bool logicTable; 			/**< true: LUT-based table; false: BRAM-based */
		double cpDelay;  				/**< For a LUT-based table, its delay; */
        Table table;
        bool full;
	public:
		int const & wOut;
		int const & wIn;
	};
} // namespace flopoco
#endif