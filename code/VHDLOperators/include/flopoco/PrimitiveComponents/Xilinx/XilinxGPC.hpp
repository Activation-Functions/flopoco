#pragma once

#include "flopoco/BitHeap/Compressor.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco
{
    class XilinxGPC : public Compressor
	{
	public:

		/**
		 * A basic constructor
		 */
        XilinxGPC(Operator *parentOp, Target * target, vector<int> heights);

		/** Factory method */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
		
	public:
        void emulate(TestCase *tc, vector<int> heights);

	private:
	};

    class BasicXilinxGPC : public BasicCompressor
	{
	public:
		BasicXilinxGPC(Operator* parentOp_, Target * target, vector<int> heights);

		virtual Compressor* getCompressor(unsigned int middleLength);


	};
}
