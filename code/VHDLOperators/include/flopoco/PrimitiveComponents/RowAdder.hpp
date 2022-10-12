#pragma once

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

#include "flopoco/UserInterface.hpp"
#include "gmp.h"
#include <gmpxx.h>
#include "mpfr.h"

#include "flopoco/BitHeap/Compressor.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"


namespace flopoco
{
    class RowAdder : public Compressor
    {
    public:
        /** constructor **/
        RowAdder(Operator *parentOp, Target *target, vector<int> _heights, vector<int> _outHeights);
        RowAdder(Operator *parentOp, Target *target, int wIn, int type=2);

        /** destructor**/
        ~RowAdder();

        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);
        static void calc_widths(int wIn, int type, vector<int> &heights, vector<int> &outHeights);

    protected:
        int wIn;
    };

    class BasicRowAdder : public BasicCompressor
    {
    public:
        BasicRowAdder(Operator* parentOp_, Target * target, int wIn, int type=2);
        virtual Compressor* getCompressor(unsigned int middleLength);
    protected:
        int wIn;
        int type;
        static vector<int> calc_heights(int wIn, int type);
    private:
        using CompressorType = BasicCompressor::CompressorType;
    };
}
