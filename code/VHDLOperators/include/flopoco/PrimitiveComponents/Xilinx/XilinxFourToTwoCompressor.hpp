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
class XilinxFourToTwoCompressor : public Compressor
{
public:
    /** constructor **/
    XilinxFourToTwoCompressor(Operator* parentOp, Target* target, int width, bool useLastColumn=true);

    /** destructor**/
    ~XilinxFourToTwoCompressor();

    virtual void setWidth(int width);

    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

private:
    bool useLastColumn;
protected:
    int width;
};

class BasicXilinxFourToTwoCompressor : public BasicCompressor
{
public:
    BasicXilinxFourToTwoCompressor(Operator* parentOp_, Target * target, int wIn);
    virtual Compressor* getCompressor(unsigned int middleLength);
    static void calc_widths(int wIn, vector<int> &heights, vector<int> &outHeights);
protected:
    int wIn;
    static vector<int> calc_heights(int wIn);
};

}
