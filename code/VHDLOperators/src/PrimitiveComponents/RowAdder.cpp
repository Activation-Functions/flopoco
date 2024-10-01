#include <iostream>
#include <sstream>
#include <string>
#include "gmp.h"
#include "mpfr.h"
#include <vector>
#include <gmpxx.h>
#include <stdio.h>
#include <stdlib.h>
#include "flopoco/PrimitiveComponents/Xilinx/XilinxGPC.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"
#include "flopoco/PrimitiveComponents/RowAdder.hpp"



using namespace std;
namespace flopoco {

    RowAdder::RowAdder(Operator *parentOp, Target *target, vector<int> _heights, vector<int> _outHeights, bool colmode) : Compressor(
            parentOp, target) {
        setCopyrightString("Andreas Boettcher");

        outHeights = _outHeights;
        heights = _heights;

        //compressors are supposed to be combinatorial
        setCombinatorial();
        setShared();

        if(colmode){
            for(int i=heights.size()-1; i>=0; i--) {
                //no need to create a signal for columns of height 0
                if (heights[i] > 0) {
                    addInput(join("X", i), heights[i]);
                    //cerr << " creating input " << join("X", i) << " width " << heights[i] << endl;
                }
            }
            addOutput("R", heights.size()+1);
        } else {
            addInput( "x_i", heights.size() );
            addInput( "y_i", heights.size() );
            if(heights[0] == 4) addInput( "z_i", heights.size() );
            addInput  ("Cin");
            for(int i=heights.size()-1; i>=0; i--) {
                //no need to create a signal for columns of height 0
                if (heights[i] > 0) {
                    vhdl << tab <<  declare(join("X", i), heights[i]) << " <= x_i" + of(i) << " & y_i" + of(i) + ((3 <= heights[i])?" & z_i" + of(i):"") + ((i == 0)?" & Cin":"") + ";" << endl;
                }
            }
            addOutput("R", heights.size());
        }

        ostringstream name;
        name << "Row_Adder_" << heights.size() ;
        setNameWithFreqAndUID(name.str());

        declare("cc_di",4*((heights.size()+4-1)/4));
        declare("cc_s",4*((heights.size()+4-1)/4));
        declare("cc_co",4*((heights.size()+4-1)/4));
        declare("cc_o",4*((heights.size()+4-1)/4));
        string carryInMapping="'0'";
        vector<string> lutInputMappings(6);
        bool previous_carry = false;
        for(unsigned i = 0; i < heights.size(); i++){
            int element_size = ((heights[i] > 2 && i != 0) || (heights[i] > 3 && i == 0))?3
                    :(((heights[i] == 2 || heights[i] == 1) && i != 0) || ((heights[i] == 3 || heights[i] == 2) && i == 0))?2:0;
            lut_op lutOp_o5, lutOp_o6;
            if (element_size == 3) {        //LUT content for a ternary element:
                lutOp_o6 = ((lut_in(0) ^ lut_in(1)) ^ (lut_in(2) ^ lut_in(3)));   //partial product
                lutOp_o5 = ((lut_in(1) & (lut_in(2) ^ lut_in(3))) | (lut_in(2) & lut_in(3)));    //carry
                if(previous_carry == true){
                    lutInputMappings = {join("c", i-1),join("X", i) + of(0),join("X", i) + of(1),join("X", i) + of(2),"'0'","'1'"};
                    vhdl << tab << "cc_di(" << i << ") <= " << join("c", i-1) << ";" << endl;
                } else {
                    lutInputMappings = {join("X", i) + of(0),join("X", i) + of(1),join("X", i) + of(2),join("X", i) + of(3),"'0'","'1'"};
                    vhdl << tab << "cc_di(" << i << ") <= " << join("X", i) + of(0) << ";" << endl;
                }
                declare(join("c", i));
                outPortMap("o5",join("c", i));
                previous_carry = true;
            } else if (element_size == 2) {     //LUT content for a RCA element:
                lutOp_o6 = (lut_in(0) ^ lut_in(1));   //partial product
                lutOp_o5 = 0;    //carry
                if(previous_carry == true){
                    lutInputMappings = {join("c", i-1),join("X", i) + of(0),"'0'","'0'","'0'","'1'"};
                    vhdl << tab << "cc_di(" << i << ") <= " << join("c", i-1) << ";" << endl;
                } else {
                    lutInputMappings = {join("X", i) + of(0),join("X", i) + of(1),"'0'","'0'","'0'","'1'"};
                    vhdl << tab << "cc_di(" << i << ") <= " << join("X", i) + of(0) << ";" << endl;
                }
                previous_carry = false;
                outPortMap("o5","open");
                carryInMapping=join("X", i) + of(2);
            } else {
                cerr << "this should not happen" << endl;
            }
            //instantiate LUT
            lut_init lutOp( lutOp_o5, lutOp_o6 );
            //cerr << lutOp.get_hex() << endl;
            Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2(this,target);
            cur_lut->setGeneric( "init", lutOp.get_hex() , 64 );
            //connect LUT inputs
            for(int j=0; j <= 5; j++)
            {
                if(lutInputMappings[j].find("'") == string::npos){   //input is a dynamic signal
                    //cerr << lutInputMappings[j] << endl;
                    inPortMap("i" + to_string(j),lutInputMappings[j]);
                } else {    //input is a constant
                    inPortMapCst("i" + to_string(j),lutInputMappings[j]);
                }
            }
            outPortMap("o6","cc_s" + of(i));
            vhdl << cur_lut->primitiveInstance(join("lut",i)) << endl;

            //fast carry chain
            if(i%4 == 0){
                Xilinx_CARRY4 *cur_cc = new Xilinx_CARRY4(this,target);
                if(i == 0){
                    inPortMapCst("ci", "'0'" );
                    if(element_size == 2){  //input is a dynamic signal
                        inPortMap("cyinit", carryInMapping);
                    } else {    //input is a constant
                        inPortMapCst("cyinit", carryInMapping);
                    }
                } else {
                    inPortMap("ci", "cc_co" + of(4*(i/4-1)+3) );
                    inPortMapCst("cyinit", "'0'");
                }

                inPortMap( "di", "cc_di" + range( 4*(i/4)+3, 4*(i/4)) );
                inPortMap( "s", "cc_s" + range( 4*(i/4)+3, 4*(i/4)) );
                outPortMap( "co", "cc_co" + range( 4*(i/4)+3, 4*(i/4)) );
                outPortMap( "o", "cc_o" + range( 4*(i/4)+3, 4*(i/4)) );

                vhdl << cur_cc->primitiveInstance(join("cc",i/4)) << endl;
            }
        }

        declare( getTarget()->adderDelay(heights.size()+1),"result", heights.size()+1+((colmode == 0)?-1:0));    //TODO: Check timing information
        vhdl << tab << "result <= " + ((colmode == 1)?"cc_co" + of(heights.size()-1) + " & ":"") + "cc_o" + range( heights.size()-1, 0 ) + ";" << endl;

        vhdl << tab << "R <= result;" << endl;

    }

    RowAdder::RowAdder(Operator *parentOp, Target *target, int wIn, int type, bool colmode) : Compressor(
            parentOp, target) {
        calc_widths(wIn, type, heights, outHeights);
        if(colmode == 0) heights.pop_back(); //remove leading element as it is currently expected when used as an adder that each input has the same height
        new (this) RowAdder(parentOp, target, heights, outHeights, colmode);
    }

    void RowAdder::calc_widths(int wIn, int type, vector<int> &heights, vector<int> &outHeights){
        vector<int> comp_inputs, comp_outputs;
        for(int i = 0; i <= wIn; i++){
            comp_outputs.push_back(1);
            if(i == 0) comp_inputs.push_back(type+1);
            if(0 < i && i < wIn) comp_inputs.push_back(type);
            if(i == wIn && type == 3){
                comp_outputs.push_back(1);
                comp_inputs.push_back(1);
            }
        }
        heights = comp_inputs;
        outHeights = comp_outputs;
    }

    vector<int> BasicRowAdder::calc_heights(int wIn, int type){
        vector<int> comp_inputs;
        for(int i = 0; i <= wIn; i++){
            if(i == 0) comp_inputs.push_back(type+1);
            if(0 < i && i < wIn) comp_inputs.push_back(type);
            if(i == wIn && type == 3) comp_inputs.push_back(1);
        }
        return comp_inputs;
    }

    BasicRowAdder::BasicRowAdder(Operator* parentOp_, Target * target, int wIn, int type) : BasicCompressor(parentOp_, target, calc_heights( wIn, type), 0, CompressorType::Variable, true), type(type)
    {
        area = wIn; //1 LUT per bit
        RowAdder::calc_widths(wIn, type, heights, outHeights);
        //cerr << heights.size() << " " << outHeights.size() << endl;
        rcType = 0;
    }

    Compressor* BasicRowAdder::getCompressor(unsigned int middleLength){
        if (middleLength >= 0) {
            area = middleLength + 2;
            RowAdder::calc_widths(middleLength+2, type, heights, outHeights);
        }

        compressor = new RowAdder(parentOp, target, heights, outHeights);
        return compressor;
    }

    RowAdder::~RowAdder() {
    }

    OperatorPtr RowAdder::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui) {
        int wIn, height;
        bool colmode;
        ui.parseInt(args, "wIn", &wIn);
        ui.parseInt(args, "height", &height);
        ui.parseBoolean(args, "colmode", &colmode);
        return new RowAdder(parentOp, target, wIn, height, colmode);
    }

    template <>
    const OperatorDescription<RowAdder> op_descriptor<RowAdder> {
	"RowAdder",		       // name
	"Row adder for cormpression.", // description, string
	"Primitives", // category, from the list defined in UserInterface.cpp
	"",
	"wIn(int): input width of the row adder;\
	height(int)=2: adder height (2 or 3); \
        colmode(bool)=1: input as column-vectors rather then row vector;",
	""};
}
