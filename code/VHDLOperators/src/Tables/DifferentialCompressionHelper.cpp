#include "flopoco/Tables/DifferentialCompressionHelper.hpp"
#include "flopoco/Tables/TableOperator.hpp"
#include <iostream>
#include <cassert>

using std::pair;
using std::make_pair;

namespace flopoco {
	using min_max_t = pair<mpz_class, mpz_class>;

	string DifferentialCompressionHelper::newUniqueInstance(OperatorPtr op,
																										string actualInputName, string actualOutputName,
																										vector<mpz_class> values, string name,
																										int wIn, int wOut, int logicTable)
	{
		if (wIn==-1 || wOut==-1){
			ostringstream o; o << " DiffCompressedTable::newUniqueInstance needs explicit wIn and wOut" << endl;
			throw o.str();
		}
		// compress
		auto t = DifferentialCompression::find_differential_compression(values, wIn, wOut, op->getTarget());
		string subsamplingOutName = actualOutputName+"_subsampling";
		string diffOutputName = actualOutputName+"_diff";
		string subsamplingIn = actualInputName+"_"+name+"_subsampling";
		// generate VHDL for subsampling table
		op->vhdl << tab << op->declare(subsamplingIn, t.subsamplingIndexSize) << " <= " << actualInputName << range(wIn-1, wIn-t.subsamplingIndexSize) << ";" << endl;
		TableOperator::newUniqueInstance( op, subsamplingIn, subsamplingOutName,
															t.subsampling, name+"_subsampling",
															t.subsamplingIndexSize, t.subsamplingWordSize, logicTable );
		// generate VHDL for diff table
		TableOperator::newUniqueInstance( op, actualInputName, diffOutputName,
															t.diffs, name+"_diff",
															wIn, t.diffWordSize );

		this->insertAdditionVHDL(op, t, actualOutputName, subsamplingOutName, diffOutputName); // TODO fix here
		return t.report(); // don't know what Operator to return, hope it is harmless here
	}


	void DifferentialCompressionHelper::insertAdditionVHDL(OperatorPtr op, DifferentialCompression t,
																									 string actualOutputName, string subsamplingOutName,string diffOutputName)
	{
		int wIn = t.diffIndexSize;
		int wOut= t.originalWout;
		int nonOverlapMSBBits = wOut- t.diffWordSize;
		int overlapMiddleBits    = t.subsamplingWordSize - nonOverlapMSBBits;
		REPORT(DETAIL, "*****************" << t.diffWordSize-overlapMiddleBits-1);
		// TODO an intadder when needed, but this is proably never useful
		op->vhdl << tab << op->declare(op->getTarget()->adderDelay(t.subsamplingWordSize),
																	 actualOutputName+"_topbits", t.subsamplingWordSize) << " <= " << subsamplingOutName
		<< " + (" << zg(nonOverlapMSBBits) << "& (" << diffOutputName << range(t.diffWordSize-1, t.diffWordSize-overlapMiddleBits) << "));" << endl;
		op->vhdl << tab << op->declare(actualOutputName, wOut) << " <= " << actualOutputName+"_topbits";
		if(t.diffWordSize-overlapMiddleBits-1 >=0 ) {
			op->vhdl << " & (" <<diffOutputName << range(t.diffWordSize-overlapMiddleBits-1,0) << ")";
		}
		op->vhdl << ";" << endl;
	}
}
