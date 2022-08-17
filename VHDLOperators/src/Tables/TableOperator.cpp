#include "flopoco/Tables/TableOperator.hpp"
#include <cassert>

namespace flopoco
{

	TableOperator::TableOperator(OperatorPtr parentOp_, Target *target_):Operator(parentOp_, target_), wIn(table.wIn),wOut(table.wOut) {

	}

	TableOperator::TableOperator(OperatorPtr parentOp_, Target *target_,
				     vector<mpz_class> _values, string _name,
				     int _wIn, int _wOut, int _logicTable,
				     int _minIn, int _maxIn)
	    : Operator(parentOp_, target_), wIn(table.wIn),wOut(table.wOut)
	{
		srcFileName = "TableOperator";
		setNameWithFreqAndUID(_name);
		setCopyrightString(
		    "Florent de Dinechin, Bogdan Pasca (2007-2020)");
		init(_values, _name, _wIn, _wOut, _logicTable, _minIn, _maxIn);
		generateVHDL();
	}

	void TableOperator::init(vector<mpz_class> & values, string _name, int _wIn, int _wOut, int _logicTable, int _minIn, int _maxIn)
	{
		table = Table{values, _wIn, _minIn, _maxIn};
		assert((_wOut == table.wOut || _wOut < 0) && "wOut is wrong");
		
		// if this is just a Table
		if (_name == "")
			setNameWithFreqAndUID(srcFileName + "_" +
					      vhdlize(table.wIn) + "_" +
					      vhdlize(table.wOut));

		// checks for logic table
		if (_logicTable == -1)
			logicTable = false;
		else if (_logicTable == 1)
			logicTable = true;
		else
			logicTable = (table.wIn <= getTarget()->lutInputs()) ||
				     (table.wOut * (mpz_class(1) << wIn) <
				      getTarget()->sizeOfMemoryBlock() / 2);
		REPORT(DEBUG, "_logicTable=" << _logicTable
					     << "  logicTable=" << logicTable);

		// Sanity check: the table is built using a RAM, but is
		// underutilized
		if (!logicTable && ((wIn <= getTarget()->lutInputs()) ||
				    (wOut * (mpz_class(1) << wIn) <
				     0.5 * getTarget()->sizeOfMemoryBlock())))
			REPORT(0, "Warning: the table is built using a RAM "
				  "block, but is underutilized");

		// Logic tables are shared by default, large tables are unique
		// because they can have a multi-cycle schedule.
		if (logicTable)
			setShared();

		// Set up the IO signals -- this must come after the setShared()
		addInput("X", wIn, true);
		addOutput("Y", wOut, 1, true);

		// determine if this is a full table
		if ((table.minIn == 0) && (table.maxIn == (1 << wIn) - 1))
			full = true;
		else
			full = false;

		// user warnings
		if (wIn > 12)
			REPORT(FULL,
			       "WARNING: FloPoCo is building a table with "
				   << wIn << " input bits, it will be large.");
	}

	void TableOperator::generateVHDL()
	{
		// create the code for the table
		REPORT(DEBUG, "Table.cpp: Filling the table");

		if (logicTable) {
			int lutsPerBit;
			if (wIn < getTarget()->lutInputs())
				lutsPerBit = 1;
			else
				lutsPerBit =
				    1 << (wIn - getTarget()->lutInputs());
			REPORT(DETAILED, "Building a logic table that uses "
					     << lutsPerBit
					     << " LUTs per output bit");
		}

		cpDelay = getTarget()->tableDelay(wIn, wOut, logicTable);
		declare(cpDelay, "Y0", wOut);
		REPORT(DEBUG, "logicTable=" << logicTable
					    << "   table delay is " << cpDelay
					    << " s");

		vhdl << tab << "with X select Y0 <= " << endl;
		;

		for (unsigned int i = table.minIn.get_ui();
		     i <= table.maxIn.get_ui(); i++)
			vhdl << tab << tab << "\""
			     << unsignedBinary(table[i - table.minIn.get_ui()],
					       wOut)
			     << "\" when \"" << unsignedBinary(i, wIn) << "\","
			     << endl;
		vhdl << tab << tab << "\"";
		for (int i = 0; i < wOut; i++)
			vhdl << "-";
		vhdl << "\" when others;" << endl;

		// TODO there seems to be several possibilities to make a BRAM;
		// the following seems ineffective
		std::string tableAttributes;
		// set the table attributes
		if (getTarget()->getID() == "Virtex6")
			tableAttributes =
			    "attribute ram_extract: string;\nattribute "
			    "ram_style: string;\nattribute ram_extract of Y0: "
			    "signal is \"yes\";\nattribute ram_style of Y0: "
			    "signal is ";
		else if (getTarget()->getID() == "Virtex5")
			tableAttributes =
			    "attribute rom_extract: string;\nattribute "
			    "rom_style: string;\nattribute rom_extract of Y0: "
			    "signal is \"yes\";\nattribute rom_style of Y0: "
			    "signal is ";
		else
			tableAttributes =
			    "attribute ram_extract: string;\nattribute "
			    "ram_style: string;\nattribute ram_extract of Y0: "
			    "signal is \"yes\";\nattribute ram_style of Y0: "
			    "signal is ";

		if ((logicTable == 1) || (wIn <= getTarget()->lutInputs())) {
			// logic
			if (getTarget()->getID() == "Virtex6")
				tableAttributes += "\"pipe_distributed\";";
			else
				tableAttributes += "\"distributed\";";
		} else {
			// block RAM
			tableAttributes += "\"block\";";
		}
		getSignalByName("Y0")->setTableAttributes(tableAttributes);
		schedule();
		vhdl << tab << declare("Y1", wOut)
		     << " <= Y0; -- for the possible blockram register" << endl;

		if (!logicTable &&
		    getTarget()
			->registerLargeTables()) { // force a register so that a
						   // blockRAM can be infered
			setSequential();
			int cycleY0 = getCycleFromSignal("Y0");
			getSignalByName("Y1")->setSchedule(cycleY0 + 1, 0);
			getSignalByName("Y0")->updateLifeSpan(1);
		}

		vhdl << tab << "Y <= Y1;" << endl;
	}

	mpz_class TableOperator::val(int x) { return table[x]; }

	int TableOperator::size_in_LUTs() const
	{
		return table.wOut *
		       int(intpow2(table.wIn - getTarget()->lutInputs()));
	}

	OperatorPtr TableOperator::newUniqueInstance(OperatorPtr op,
						     string actualInput,
						     string actualOutput,
						     vector<mpz_class> values,
						     string name, int wIn,
						     int wOut, int logicTable)
	{
		op->schedule();
		op->inPortMap("X", actualInput);
		op->outPortMap("Y", actualOutput);
		auto *t = new TableOperator(op, op->getTarget(), values, name,
					    wIn, wOut, logicTable);

		op->vhdl << op->instance(t, name, false);
		return t;
	}

} // namespace flopoco