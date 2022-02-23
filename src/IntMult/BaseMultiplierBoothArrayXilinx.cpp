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
    string in1,in2, in3;
    int width, height;
    bool in2_signed, in1_signed;

    in1 = "X";
    in2 = "Y";
    in3 = "Tin";
    name << "BaseMultiplier" << wX << "x" << wY;
    width = wX;
    height = wY;

    in2_signed = isSignedY;         //Y is the short side
    in1_signed = isSignedX;         //X is the long side

    bool heightparity = height%2;
    int stages = height/2+1;
    int slices = ceil(float(width)/4+1);
    int needed_luts = slices*4;//no. of required LUTs
    int needed_cc = slices; //no. of required carry chains

    setNameWithFreqAndUID(name.str());

    addInput("X", wX, true);
    addInput("Y", wY, true);
    addInput("Tin", wX+1, true);

    addOutput("R", wX+wY+1, 1, true);

    //if(isSignedX || isSignedY) throw string("Signed multiplication is not supported yet!");


    for(int j = 0; j < stages; j++){

            declare(join("t", j), needed_cc * 4);
            //declare( join("sin", j), needed_cc * 4 * 6);

            declare(join("cc_s", j), needed_cc * 4);
            declare(join("cc_di", j), needed_cc * 4);
            declare(join("cc_co", j), needed_cc * 4);
            declare(join("cc_o", j), needed_cc * 4);
            vhdl << tab << join("cc_di", j, " <= ") << join("t", j) << ";" << endl;

            vhdl << tab << join("t", j, "(", width + 3, ") <= '1';") << endl;
            if (j == 0) {
                vhdl << tab << join("t0(", width + 2, ") <= '1';") << endl;
                if (!useAccumulate) {
                    vhdl << tab << join("t0(", width + 1, " downto ", 0, ") <= (others => '0');") << endl;
                } else{
                    vhdl << tab << join("t0(", width + 1, " downto ", 1, ") <= Tin;") << endl;
                    vhdl << tab << join("t0(", 0, ") <= '0';") << endl;
                }
            } else {
                vhdl << tab << join("t", j, "(0) <= '0';") << endl;
            }
        if (!isSignedY||j<stages-1||heightparity) {


            //create the LUTs:
            for (int i = 0; i < needed_luts; i++) {
                //LUT content of the LUTs:
                lut_op lutab;
                lut_op z = ((~lut_in(0) & ~lut_in(1) & ~lut_in(2)) | (lut_in(0) & lut_in(1) & lut_in(2)));
                //lut_op c = ((lut_in(0));
                lut_op c = ((lut_in(0) & ~(lut_in(1)) | (lut_in(0) & ~lut_in(2))));
                lut_op s = ((~lut_in(0) & lut_in(1) & lut_in(2)) | (lut_in(0) & ~lut_in(1) & ~lut_in(2)));
                lut_op e = (c ^lut_in(4)) & ~z;
                lut_op mux0 = (~s & lut_in(4) | s & lut_in(5));
                lut_op mux1 = (~c & mux0 | c & ~mux0);
                lut_op mux2 = ~z & mux1;
                if (i < width + 2) {                    //Mapping A
                    lutab = lut_in(3) ^ mux2;
                } else if (i == width + 2 & (!yIsSigned && !xIsSigned)) {          //Mapping B
                    lutab = lut_in(3) ^ ~c;
                } else if (i == width + 2 & (xIsSigned || yIsSigned)) {          //Mapping B (negative)
                    lutab = lut_in(3) ^ ~e;
                } else if (i == width + 3) {                                    //Mapping C
                    lutab = lut_in(3);
                }
                lut_init lutop(lutab);

                Xilinx_LUT6 *cur_lut = new Xilinx_LUT6(this, target);
                cur_lut->setGeneric("init", lutop.get_hex(), 64);

                /*           vhdl << join("sin", j) + range( i*6+5, i*6) << " <= "
                            << ((j*2+1 < height)?"in2" + of(j*2+1):"'0'") << " & "
                            << ((j*2+0 < height)?"in2" + of(j*2+0):"'0'") << " & "
                            << ((j*2-1 >= 0    )?"in2" + of(j*2-1):"'0'") << " & "
                            << ((0 < i)?join("t", j) + of(i):"'0'") << " & "
                            << ((0 < i && i <= width+0)?"in1" + of(i-1):"'0'") << " & "
                            << ((1 < i && i <= width+1)?"in1" + of(i-2):"'0'") << ";" << endl;*/
                //vhdl << join("sin", j) + range( i*6+5, i*6) << " <= cc_o(" << 5 << " downto 0);" << endl;

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
                if (i == 0 || i > width + 3) {
                    inPortMapCst("i3", "'0'");
                } else if (i == width + 2 && j == 0 || i == width + 3) {
                    inPortMapCst("i3", "'1'");
                } else {
                    inPortMap("i3", join("t", j) + of(i));
                }
                if (0 < i && i <= width ) {
                    inPortMap("i4", in1 + of(i - 1));
                } else if (i == width + 1 && (isSignedX)) {
                    inPortMap("i4", in1 + of(i - 2));
                } else if (i == width + 2 && (isSignedX)) {
                    inPortMap("i4", in1 + of(i - 3));
                } else {
                    inPortMapCst("i4", "'0'");
                }
                if (1 < i && i <= width + 1) {
                    inPortMap("i5", in1 + of(i - 2));
                } else {
                    inPortMapCst("i5", "'0'");
                }



                //outPortMap("o5",join("cc_di", j) + of(i));
                outPortMap("o", join("cc_s", j) + of(i));
                vhdl << cur_lut->primitiveInstance(join("lut", j, "_", i)) << endl;
            }

            //create the carry chain:
            for (int i = 0; i < needed_cc; i++) {
                Xilinx_CARRY4 *cur_cc = new Xilinx_CARRY4(this, target);

                inPortMapCst("cyinit", "'0'");
                if (i == 0) {
                    inPortMapCst("ci", "'1'"); //carry-in can not be used as AX input is blocked!!
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

            for (int i = 0; i < slices * 4; i++) {
                if (j < stages - 1) {
                    if (i == 1 || i == 2) {
                        vhdl << tab << join("R(", (i - 1) + j * 2, ")<=") << join("cc_o", j, "(") << i << ");" << endl;
                    } else {
                        if (i > 2)
                            vhdl << tab << join("t", j + 1, "(", i - 2, ")<=") << join("cc_o", j, "(") << i << ");"
                                 << endl;
                    }
                    if (i == width + 3) {
                        vhdl << tab << join("t", j + 1, "(", i - 1, ")<=") << join("cc_co", j, "(") << i << ");"
                             << endl;
                    }
                }

            }

            vhdl << endl;
             }
    }
    //vhdl << tab << join("R(",(stages-1)*2+needed_luts-1," downto ", (stages-1)*2,") <= ") << join("cc_co", stages-1) << "(" << width+3 << join(") & cc_o",stages-1,"(") << width+3 << " downto 1);" << endl;         //needs change
    //vhdl << tab << join("R(",wX+wY+1," downto ", (stages-1)*2,") <= ") << join("not cc_co", stages-1) << "(" << width+3 << join(") & cc_o",stages-1,"(") << width+3 << " downto 1);" << endl;
    if((isSignedY)&& !heightparity){
        vhdl << tab << join("R(",wX+wY," downto ", (stages-1)*2,") <= ") << join(" cc_o",stages-2,"(") << width+wY%2+3 << " downto 3);" << endl;
    }else{
        vhdl << tab << join("R(",wX+wY," downto ", (stages-1)*2,") <= ") << join(" cc_o",stages-1,"(") << width+wY%2+1 << " downto 1);" << endl;
    }
    //vhdl << tab << join("R(",(stages-1)*2+needed_luts-2," downto ", (stages-1)*2,") <= ") << join("cc_o",stages-1,"(") << width+3 << " downto 1);" << endl;


    //vhdl << tab << "R <= " << join("cc_co", stages-1) << "(" << width << ") & cc_o(" << width << " downto 0);" << endl;         //needs change

}

}   //end namespace flopoco
