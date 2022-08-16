#include "flopoco/IntMult/BaseSquarerLUT.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"
#include "flopoco/Tables/Table.hpp"
#include "flopoco/Tables/TableOperator.hpp"

namespace flopoco
{

    BaseSquarerLUTOp::BaseSquarerLUTOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, int wIn) : Operator(parentOp,target), wIn(wIn), isSigned(isSignedX || isSignedY)
    {
        int wInMax = (isSigned)?(-1)*(1<<(wIn-1)):(1<<wIn)-1;
        int wR = ceil(log2(wInMax*wInMax+1));

        ostringstream name;
        name << "BaseSquarerLUT_" << wIn << "x" << wIn  << (isSigned==1 ? "_signed" : "");

        setNameWithFreqAndUID(name.str());

        addInput("X", wIn);
        addOutput("R", wR);

        if(wIn == 1){
            vhdl << tab << "R <= X;" << endl;
        } else if (target->getVendor() != "Xilinx" && 1 < wIn && wIn < 6){
            vector<mpz_class> tabH;
            REPORT(DEBUG, "Filling table for a LUT squarer of size " << wIn << "x" << wIn << " (out put size is " << wR << ")")
            for (int yx=0; yx < 1<<(wIn); yx++)
            {
                tabH.push_back((function(yx)>>2)&((1<<(wR-2))-1));
            }
            Operator *opH = new TableOperator(this, target, tabH, "SquareTable", wIn, wR-2);
            opH->setShared();
            UserInterface::addToGlobalOpList(opH);
            vhdl << declare(0.0,"XtableH",wIn) << " <= X;" << endl;
            inPortMap("X", "XtableH");
            outPortMap("Y", "Y1H");
            vhdl << instance(opH, "TableSquarerH");

            vhdl << tab << "R <= Y1H & \"0\" & X(0);" << endl;
        } else if(target->getVendor() == "Xilinx" && 1 < wIn && wIn <= 6) {
            //build the logic equations for the LUT-mappings based on the cases when the respective output bits are 1
            declare("y", wR-2);
            vector<lut_op> lutMappings(2*wIn-2);
            int lutnr = 0;
            for(int o = 0; o < 2*wIn-2; o++){
                int firstTerm = 0, l6m = ((wIn == 6 && 2*wIn-5 < o && !isSigned)?1:0);
                for (int yx=0; yx < 1<<(wIn); yx++)
                {
                    mpz_class temp = (function(yx)>>2);
                    if((temp & (1<<o)) != 0){
                        lut_op term;
                        int notFirstInTerm = 0, contrib = 0;
                        for(int i = l6m; i < wIn; i++){
                            if(yx & (1<<i)){
                                if(notFirstInTerm++){
                                    term = term & lut_in(i-l6m);
                                } else {
                                    term = lut_in(i-l6m);
                                }
                                contrib++;
                            } else {
                                if(notFirstInTerm++){
                                    term = term & ~lut_in(i-l6m);
                                } else {
                                    term = ~lut_in(i-l6m);
                                }
                            }
                        }
                        if(contrib && firstTerm++){
                            lutMappings[o] = lutMappings[o] | term;
                        } else {
                            lutMappings[o] = term;
                        }
                    }
                }
                //lutMappings[o].print();

                //instantiate LUT
                if(wIn == 6 && 3 < o && (o < 2*wIn-4 || (2*wIn-5 < o && isSigned)) ){   //The 6x6 squarer has some output eq. that only fit in a 6LUT as they depend on 6 inputs
                    if(isSigned && 2*wIn-4 < o) break;
                    declare(target->logicDelay(6) + 9e-10, join("lut", lutnr, "_o"));
                    lut_init lutOp( lutMappings[o]);
                    //cout << lutOp.get_hex() << endl;
                    Xilinx_LUT6 *cur_lut = new Xilinx_LUT6(this,target);
                    cur_lut->setGeneric( "init", lutOp.get_hex() , 64 );

                    //connect LUT inputs
                    for(int j=0; j < 6; j++)
                        inPortMap("i" + to_string(j), "X" + of(j));
                    outPortMap("o",join("lut", lutnr, "_o"));
                    vhdl << cur_lut->primitiveInstance(join("lut_n", lutnr)) << endl;
                    vhdl << tab << "y" + of(o) << "<=" << join("lut", lutnr, "_o") << ";" << endl;
                } else {    //The 5LUTs for all other cases are generated here
                    if(o%2 == 1){
                        declare(target->logicDelay(6) + 9e-10, join("lut_n", lutnr, "_o6"));
                        declare(target->logicDelay(6) + 9e-10, join("lut_n", lutnr, "_o5"));
                        lut_init lutOp( lutMappings[o-1], lutMappings[o]);
                        //cout << lutOp.get_hex() << endl;
                        Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2(this,target);
                        cur_lut->setGeneric( "init", lutOp.get_hex() , 64 );

                        //connect LUT inputs
                        for(int j=0; j < 6; j++)
                        {
                            if(j < wIn-((wIn==6)?1:0)){
                                inPortMap("i" + to_string(j), "X" + of(j+l6m));
                            } else {    //input is a constant
                                if(j < 5)
                                    inPortMapCst("i" + to_string(j), "'0'");
                                else
                                    inPortMapCst("i" + to_string(j), "'1'");
                            }
                        }
                        outPortMap("o5",join("lut", lutnr, "_o5"));
                        if(isSigned && o == 2*wIn-3)
                            outPortMap("o6","open");
                        else
                            outPortMap("o6",join("lut", lutnr, "_o6"));
                        vhdl << cur_lut->primitiveInstance(join("lut_n", lutnr)) << endl;
                        vhdl << tab << "y" + of(o-1) << "<=" << join("lut", lutnr, "_o5") << ";" << endl;
                        vhdl << tab << "y" + of(o) << "<=" << join("lut", lutnr, "_o6") << ";" << endl << endl;
                    }
                }
                lutnr++;
            }
            vhdl << tab << "R <= y & \"0\" & X(0);" << endl;

        } else if(target->getVendor() != "Xilinx" && wIn == 6){    //In the 6x6-case 4 LUTs can be saved, because only bits 11-6 need to be mapped in 6LUTs, 5-2 fit in 5LUTs using subdivision of the 6LUTs, hence (8 6-LUTs total)
            vector<mpz_class> tabH, tabM;
            REPORT(DEBUG, "Filling table for a LUT squarer of size " << wIn << "x" << wIn << " (out put size is " << wR << ")")
            for (int yx=0; yx < 1<<(wIn); yx++)
            {
                mpz_class temp = function(yx);
                tabH.push_back((temp>>6)&0x3F);
                if(yx < 1<<(wIn-1))
                    tabM.push_back((temp>>2)&0x0F);
            }
            Operator *opH = new TableOperator(this, target, tabH, "SquareTable", wIn, 6-((isSigned)?1:0));
            opH->setShared();
            UserInterface::addToGlobalOpList(opH);
            vhdl << declare(0.0,"XtableH",wIn) << " <= X;" << endl;
            inPortMap("X", "XtableH");
            outPortMap("Y", "Y1H");
            vhdl << instance(opH, "TableSquarerH");


            Operator *opM = new TableOperator(this, target, tabM, "SquareTable", 5, 4);
            opM->setShared();
            UserInterface::addToGlobalOpList(opM);
            vhdl << declare(0.0,"XtableM",5) << " <= X(4 downto 0);" << endl;
            inPortMap("X", "XtableM");
            outPortMap("Y", "Y1M");
            vhdl << instance(opM, "TableSquarerM");

            vhdl << tab << "R <= Y1H & Y1M & \"0\" & X(0);" << endl;
        } else {    //Squarers larger then 6x6 are realized with a table
            vector<mpz_class> val;
            REPORT(DEBUG, "Filling table for a LUT squarer of size " << wIn << "x" << wIn << " (out put size is " << wR << ")")
            for (int yx=0; yx < 1<<(wIn); yx++)
            {
                val.push_back(function(yx));
            }
            Operator *op = new TableOperator(this, target, val, "SquareTable", wIn, wR);
            op->setShared();
            UserInterface::addToGlobalOpList(op);

            vhdl << declare(0.0,"Xtable",wIn) << " <= X;" << endl;

            inPortMap("X", "Xtable");
            outPortMap("Y", "Y1");

            vhdl << tab << "R <= Y1;" << endl;
            vhdl << instance(op, "TableSquarer");
        }
    }

    mpz_class BaseSquarerLUTOp::function(int yx)
    {
        mpz_class r;
        if(isSigned && yx >= (1 << (wIn-1))){
            yx -= (1 << wIn);
        }
        r = yx * yx;
        REPORT(DEBUG, "Value for x=" << yx << ", x=" << yx << " : " << r.get_str(2))
        return r;
    }

    OperatorPtr BaseSquarerLUT::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
    {
        int wIn;
        bool singedness;

        UserInterface::parseStrictlyPositiveInt(args, "wIn", &wIn);
        UserInterface::parseBoolean(args, "isSigned", &singedness);

        return new BaseSquarerLUTOp(parentOp,target, singedness, singedness, wIn);
    }

    void BaseSquarerLUT::registerFactory(){
        UserInterface::add("IntSquarerLUT", // name
                           "Implements a LUT squarer by simply tabulating all results in the LUT, should only be used for very small word sizes",
                           "BasicInteger", // categories
                           "",
                           "wIn(int): size of the input XxX; isSigned(bool): signedness of the input",
                           "",
                           BaseSquarerLUT::parseArguments,
                           BaseSquarerLUTOp::unitTest
                           ) ;
    }

    double BaseSquarerLUT::getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) {
        int wInMax = (signedIO)?(-1)*(1<<(wIn-1)):(1<<wIn)-1;
        int wR = ceil(log2(wInMax*wInMax+1));
        cout << "wR=" << wR << endl;
        if(wIn<7){
            return (double)((wIn==6)?7:wIn-1) + wR*getBitHeapCompressionCostperBit();
        } else {
            return (double)(((wIn==1)?0:(wR<6)?ceil(wR/2.0):wR)) + wR*getBitHeapCompressionCostperBit();
        }
    }

    int BaseSquarerLUT::ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) {
        if(wIn<7){
            return ((wIn==6)?7:wIn-1);
        } else {
            int wInMax = (signedIO)?(-1)*(1<<(wIn-1)):(1<<wIn)-1;
            int wR = ceil(log2(wInMax*wInMax+1));
            return (((wIn==1)?0:(wR<6)?(int)ceil(wR/2.0):wR));
        }
    }

    unsigned BaseSquarerLUT::getArea(void) {
        return wIn*wIn;
    }

    bool BaseSquarerLUT::shapeValid(int x, int y)
    {
        if(0 <= x && x < wIn && 0 <= y && y < wIn && y <= x) return true;
        return false;
    }

    bool BaseSquarerLUT::shapeValid(Parametrization const& param, unsigned x, unsigned y) const
    {
        if(0 < param.getTileXWordSize() && x < param.getTileXWordSize() && 0 < param.getTileYWordSize() && y < param.getTileYWordSize() && y <= x) return true;
        return false;
    }

    Operator *BaseSquarerLUT::generateOperator(Operator *parentOp, Target *target,
                                               const BaseMultiplierCategory::Parametrization &params) const {
        return new BaseSquarerLUTOp(
                parentOp,
                target,
                params.isSignedMultX(),
                params.isSignedMultY(),
                params.getMultXWordSize()
                );
    }

    int BaseSquarerLUT::getRelativeResultMSBWeight(const BaseMultiplierCategory::Parametrization &param) const {
        int wInMax = (param.isSignedMultX() || param.isSignedMultY())?(-1)*(1<<(wIn-1)):(1<<wIn)-1;
        int wR = ceil(log2(wInMax*wInMax+1));
        return wR-1;
    }

    void BaseSquarerLUTOp::emulate(TestCase* tc)
    {
        mpz_class svX = tc->getInputValue("X");
        if(isSigned && svX >= (1 << (wIn-1))){
            svX -= (1 << wIn);
        }
        mpz_class svR = svX * svX;
        tc->addExpectedOutput("R", svR);
    }

    TestList BaseSquarerLUTOp::unitTest(int)
    {
        // the static list of mandatory tests
        TestList testStateList;
        vector<pair<string,string>> paramList;

        //test square multiplications:
        for(int w=1; w <= 6; w++)
        {
            for(int sign=0; sign < 2; sign++){
                paramList.push_back(make_pair("wIn", to_string(w)));
                paramList.push_back(make_pair("isSigned", to_string((bool)sign)));
                testStateList.push_back(paramList);
                paramList.clear();
            }
        }

        return testStateList;
    }
}
