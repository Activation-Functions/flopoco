#include "BaseMultiplierBoothArrayXilinx.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "../PrimitiveComponents/Xilinx/Xilinx_LUT_compute.h"

namespace flopoco {

//copied from 2xk
Operator* BaseMultiplierBoothArrayXilinx::generateOperator(
		Operator *parentOp,
		Target* target,
		Parametrization const & parameters) const
{
	return new BaseMultiplierBoothArrayXilinxOp(
			parentOp,
			target,
            parameters.isSignedMultX(),
			parameters.isSignedMultY(),
            parameters.getMultXWordSize(),
            parameters.getMultYWordSize()
		);
}

//may or may not be working as intended; check if copied from 2xk
//copied from 2xk
double BaseMultiplierBoothArrayXilinx::getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO){
    int luts = ((wX() < wY())?wY():wX()) + 1;

    int x_min = ((x_anchor < 0)?0: x_anchor);
    int y_min = ((y_anchor < 0)?0: y_anchor);
    int lsb = x_min + y_min;

    int x_max = ((wMultX < x_anchor + (int)wX())?wMultX: x_anchor + wX());
    int y_max = ((wMultY < y_anchor + (int)wY())?wMultY: y_anchor + wY());
    int msb = (x_max==1)?y_max:((y_max==1)?x_max:x_max+y_max);

    if(signedIO && ((wMultX-x_anchor-(int)wX())== 0 || (wMultY-y_anchor-(int)wY())== 0)){
        luts++;    //The 2xk-multiplier needs one additional LUT in the signed case
    }

    return luts + (msb - lsb)*getBitHeapCompressionCostperBit();
}

//may or may not be working as intended; check if copied from 2xk
//copied from 2xk
int BaseMultiplierBoothArrayXilinx::ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) {
    int luts = ((wX() < wY())?wY():wX()) + 1;
    if(signedIO && ((wMultX-x_anchor-(int)wX())== 0 || (wMultY-y_anchor-(int)wY())== 0)){
        luts++;    //The 2xk-multiplier needs one additional LUT in the signed case
    }
    return luts;
}

//copied from 2xk
OperatorPtr BaseMultiplierBoothArrayXilinx::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args)
{
    int wX, wY;
	bool xIsSigned,yIsSigned,useAccumulate;
    UserInterface::parseStrictlyPositiveInt(args, "wX", &wX);
    UserInterface::parseStrictlyPositiveInt(args, "wY", &wY);
	UserInterface::parseBoolean(args,"xIsSigned",&xIsSigned);
	UserInterface::parseBoolean(args,"yIsSigned",&yIsSigned);
    UserInterface::parseBoolean(args, "useAccumulate", &useAccumulate);

	return new BaseMultiplierBoothArrayXilinxOp(parentOp,target,xIsSigned,yIsSigned, wX, wY, useAccumulate);
}

void BaseMultiplierBoothArrayXilinx::registerFactory()
{
    UserInterface::add("BaseMultiplierBoothArrayXilinx", // name
                        "Implements a 2xY-LUT-Multiplier that can be realized efficiently on some Xilinx-FPGAs",
                       "BasicInteger", // categories
                        "",
                       "wX(int): size of input X;\
                        wY(int): size of input Y;\
						xIsSigned(bool)=0: input X is signed;\
						yIsSigned(bool)=0: input Y is signed;",
                       "",
                       BaseMultiplierBoothArrayXilinx::parseArguments,
                       BaseMultiplierBoothArrayXilinx::unitTest
    ) ;
}

//copied from 2xk
void BaseMultiplierBoothArrayXilinxOp::emulate(TestCase* tc)
{


    mpz_class svX = tc->getInputValue("X");
    mpz_class svY = tc->getInputValue("Y");
    mpz_class svT = tc->getInputValue("Tin");


    if(xIsSigned)
    {
        // Manage signed digits
        mpz_class big1 = (mpz_class{1} << (wX));
        mpz_class big1P = (mpz_class{1} << (wX-1));

        if(svX >= big1P) {
            svX -= big1;
            //cerr << "X is neg. Interpreted value : " << svX.get_str(10) << endl;
        }


    }
    if(yIsSigned)
    {

        mpz_class big2 = (mpz_class{1} << (wY));
        mpz_class big2P = (mpz_class{1} << (wY-1));

        if(svY >= big2P) {
            svY -= big2;
            //cerr << "Y is neg. Interpreted value : " << svY.get_str(10) << endl;
        }
    }


    //cerr << "Computed product : " << svR.get_str() << endl;
    mpz_class svR=0;

    if  (accumulateUsed){
         svR = svX * svY + svT;
    }else{
        svR = svX * svY;
    }
    tc->addExpectedOutput("R", svR);
}

//copied from 2xk
TestList BaseMultiplierBoothArrayXilinx::unitTest(int index)
{
    // the static list of mandatory tests
    TestList testStateList;
    vector<pair<string,string>> paramList;

    //test square multiplications:
    for(int w=1; w <= 6; w++)
    {
        paramList.push_back(make_pair("wY", to_string(w)));
        paramList.push_back(make_pair("wX", to_string(2)));
        testStateList.push_back(paramList);
        paramList.clear();
    }
    for(int w=1; w <= 6; w++)
    {
        paramList.push_back(make_pair("wX", to_string(w)));
        paramList.push_back(make_pair("wY", to_string(2)));
        testStateList.push_back(paramList);
        paramList.clear();
    }

    return testStateList;
}

    BaseMultiplierBoothArrayXilinxOp::BaseMultiplierBoothArrayXilinxOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, int wX, int wY, bool useAccumulate) : Operator(parentOp,target),xIsSigned(isSignedX),yIsSigned(isSignedY),wX(wX),wY(wY), accumulateUsed(useAccumulate)
    {
        //Für signed checke http://i.stanford.edu/pub/cstr/reports/csl/tr/94/617/CSL-TR-94-617.appendix.pdf
        //ZWEIERKOMPLEMENT
        ostringstream name;
        string in1,in2,in3;
        int width, height;
        //bool in2_signed, in1_signed;

        in1 = "X";
        in2 = "Y";
        in3 = "Tin";
        name << "BaseMultiplier" << wX << "x" << wY;
        width = wX;
        height = wY;

        //in2_signed = isSignedY;         //Y is the short side
        //in1_signed = isSignedX;         //X is the long side

        bool heightparity = height%2;
        int stages = height/2+1;
        //int slices = ceil(float(width)/4+1);
        int slices = ceil(float(width+2)/4);
        int needed_luts = slices*4;//no. of required LUTs
        int needed_cc = slices; //no. of required carry chains

        setNameWithFreqAndUID(name.str());

        addInput("X", wX, true);
        addInput("Y", wY, true);
        addInput("Tin", wX+1, true);
        if (useAccumulate && wX >= wY) {
            addOutput("R", wX + wY + 1, 1, true);
        } else{
            addOutput("R", wX + wY , 1, true);
        }

        for(int j = 0; j < stages; j++){

            declare(join("t", j), needed_cc * 4);
            declare(join("cc_s", j), needed_cc * 4);
            declare(join("cc_di", j), needed_cc * 4);
            declare(join("cc_co", j), needed_cc * 4);
            declare(join("cc_o", j), needed_cc * 4);

            vhdl << tab << join("cc_di", j, " <= ") << join("t", j) << ";" << endl;

            if (j == 0) {
                vhdl << tab << join("t0(", width + 1, ") <= '1';") << endl;
                if (!useAccumulate) {
                    vhdl << tab << join("t0(", width, " downto ", 0, ") <= (others => '0');") << endl;
                } else{
                    vhdl << tab << join("t0(", width , " downto ", 0, ") <= Tin;") << endl;
                }
            }
            if (!isSignedY||j<stages-1||heightparity) {


                //create the LUTs:
                for (int i = 0; i < needed_luts; i++) {
                    //LUT content of the LUTs:
                    lut_op lutab;
                    lut_op z = ((~lut_in(0) & ~lut_in(1) & ~lut_in(2)) | (lut_in(0) & lut_in(1) & lut_in(2)));
                    lut_op c = ((lut_in(0)));
                    //lut_op c = ((lut_in(0) & ~(lut_in(1)) | (lut_in(0) & ~lut_in(2))));
                    lut_op s = ((~lut_in(0) & lut_in(1) & lut_in(2)) | (lut_in(0) & ~lut_in(1) & ~lut_in(2)));
                    lut_op e = (c ^lut_in(4)) & ~z ^(c & z);
                    lut_op mux0 = (~s & lut_in(4) | s & lut_in(5));
                    lut_op mux1 = (~c & mux0 | c & ~mux0);
                    lut_op mux2 = ~z & mux1;
                    lut_op czflip = mux2 ^ (c & z);


                    if (i < width + 1) {                                              //Mapping A
                         lutab = lut_in(3) ^ czflip;
                    }else if (i == width + 1 & (!yIsSigned && !xIsSigned)) {          //Mapping B
                        lutab = lut_in(3) ^ ~c;
                    } else if (i == width + 1 & (xIsSigned || yIsSigned)) {          //Mapping B (negative)
                        lutab = lut_in(3) ^ ~e;
                    }




                    lut_init lutop(lutab);

                    Xilinx_LUT6 *cur_lut = new Xilinx_LUT6(this, target);
                    cur_lut->setGeneric("init", lutop.get_hex(), 64);


                    if (j * 2 + 1 < height) {
                        inPortMap("i0", in2 + of(j * 2 + 1));
                    } else if (heightparity&&(isSignedY)) {
                        inPortMap("i0", in2 + of(j * 2 + 0));
                    } else {
                        inPortMapCst("i0", "'0'");
                    }
                    if (j * 2 + 0 < height) {
                        inPortMap("i1", in2 + of(j * 2 + 0));
                    }else if (j * 2 + 1<height&&(isSignedY)) {
                        inPortMap("i1", in2 + of(j * 2 + 0));
                    } else {
                        inPortMapCst("i1", "'0'");
                    }
                    if (j * 2 - 1 >= 0) {
                        inPortMap("i2", in2 + of(j * 2 - 1));
                    } else {
                        inPortMapCst("i2", "'0'");
                    }
                    if (i > width + 2) {
                        inPortMapCst("i3", "'0'");
                    } else {
                        inPortMap("i3", join("t", j) + of(i));
                    }
                    if (0 <= i && i <= width - 1 ) {
                        inPortMap("i4", in1 + of(i));
                    } else if (i == width && (isSignedX)) {
                        inPortMap("i4", in1 + of(i - 1));
                    } else if (i == width + 1 && (isSignedX)) {
                        inPortMap("i4", in1 + of(i - 2));
                    } else {
                        inPortMapCst("i4", "'0'");
                    }
                    if (0 < i && i <= width ) {
                        inPortMap("i5", in1 + of(i - 1));
                    } else {
                        inPortMapCst("i5", "'0'");
                    }

                    outPortMap("o", join("cc_s", j) + of(i));
                    vhdl << cur_lut->primitiveInstance(join("lut", j, "_", i)) << endl;
                }

                //create the carry chain:
                for (int i = 0; i < needed_cc; i++) {
                    Xilinx_CARRY4 *cur_cc = new Xilinx_CARRY4(this, target);

                    inPortMapCst("cyinit", "'0'");
                    if (i == 0) {
                        if (j*2+1>wY-1) {
                            if (isSignedY){
                                inPortMap("ci", join("Y(", wY-1, ")"));
                            }else {
                                inPortMapCst("ci", "'0'");
                            }
                        }else{
                            inPortMap("ci", join("Y(", j * 2 + 1, ")"));
                        }


                    } else {
                        inPortMap("ci", join("cc_co", j) + of(i * 4 - 1));
                    }
                    inPortMap("di", join("cc_di", j) + range(i * 4 + 3, i * 4));
                    inPortMap("s", join("cc_s", j) + range(i * 4 + 3, i * 4));
                    outPortMap("co", join("cc_co", j) + range(i * 4 + 3, i * 4));
                    outPortMap("o", join("cc_o", j) + range(i * 4 + 3, i * 4));

                    vhdl << cur_cc->primitiveInstance(join("cc_", j, "_", i));

                }
                vhdl << endl;
                if (j < stages - 1) {
                    for (int i = 0; i < slices * 4; i++) {
                        if (i == 0 || i == 1) {
                            vhdl << tab << join("R(", (i) + j * 2, ")<=") << join("cc_o", j, "(") << i << ");" << endl;
                        } else {
                            if (i > 1 && i < width +2)
                                vhdl << tab << join("t", j + 1, "(", i - 2, ")<=") << join("cc_o", j, "(") << i << ");"
                                     << endl;
                        }
                    }
                    vhdl << tab << join("t", j + 1, "(", width, ")<=") << join("not cc_co", j, "(") << width + 1 << ");"
                         << endl;
                    vhdl << tab << join("t", j + 1, "(", width + 1, ")<=") << join("cc_co", j, "(") << width + 1 << ");"
                         << endl;
                }
                vhdl << endl;
            }
        }
        if((isSignedY) && !heightparity){
            if (useAccumulate && wX >= wY) {
                vhdl << tab << join("R(",wX+wY,") <= ") << join("not cc_co",stages-2,"(") << width+wY%2+1 << ");" << endl;
            }
            vhdl << tab << join("R(",wX+wY-1," downto ", (stages-1)*2,") <= ") << join(" cc_o",stages-2,"(") << width+1 << " downto 2);" << endl;
        }else{
            if (useAccumulate && wX >= wY) {
                vhdl << tab << join("R(",wX+wY,") <= ") << join(" cc_o",stages-1,"(") << width+wY%2 << ");" << endl;
            }
            vhdl << tab << join("R(",wX+wY-1," downto ", (stages-1)*2,") <= ") << join(" cc_o",stages-1,"(") << width+wY%2-1 << " downto 0);" << endl;
        }
    }

}   //end namespace flopoco
