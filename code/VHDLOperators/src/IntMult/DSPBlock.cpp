#include "flopoco/IntMult/DSPBlock.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"

namespace flopoco {

DSPBlock::DSPBlock(Operator *parentOp, Target* target, int wX, int wY, bool xIsSigned, bool yIsSigned, bool isPipelined, int wZ, bool usePostAdder, bool usePreAdder, bool preAdderSubtracts) : Operator(parentOp,target), xIsSigned_{xIsSigned}, yIsSigned_{yIsSigned}, wX_{wX}, wY_{wY}
{
	useNumericStd();

	ostringstream name;
//	name << "DSPBlock_" << wX << "x" << wY << (usePreAdder==1 ? "_PreAdd" : "") << (usePostAdder==1 ? "_PostAdd" : "") << (isPipelined==1 ? "_pip" : "") << "_uid" << getNewUId();
	name << "DSPBlock_" << wX << "x" << wY << (usePreAdder==1 && !preAdderSubtracts ? "_PreAdd" : usePreAdder==1 && preAdderSubtracts ? "_PreSub" : "") << (usePostAdder==1 ? "_PostAdd" : "") << (isPipelined==1 ? "_pip" : "");
//	setShared(); //set this operator to be a shared operator, does not work for pipelined ones!!
//	setSequential(); //Dirty hack!!

	setNameWithFreqAndUID(name.str());

	if(wZ == 0 && usePostAdder) THROWERROR("usePostAdder was set to true but no word size for input Z was given.");

	double maxTargetCriticalPath = 1.0 / getTarget()->frequency() - getTarget()->ffDelay();

	double stageDelay = 0.0;      //TODO Check what would be a correct value if Block is not pipelined

	if(isPipelined) stageDelay = 0.9 * maxTargetCriticalPath;

	bool signedMultOutput = xIsSigned or yIsSigned;
	bool oneOnlySigned = xIsSigned xor yIsSigned;
	int onlyOneDelta = (oneOnlySigned) ? 1 : 0;

	bool shouldPadX = oneOnlySigned and yIsSigned;
	bool shouldPadY = oneOnlySigned and xIsSigned;

	if(usePreAdder)
	{
		addInput("X1", wX);
		addInput("X2", wX);
	}
	else
	{
		addInput("X", wX);
	}

	addInput("Y", wY);

	int wM; //the width of the multiplier result
	if(usePreAdder)
	{

		//implement pre-adder:
		vhdl << tab << declare(getTarget()->DSPAdderDelay(),"X",wX) << " <= std_logic_vector(" << (xIsSigned ? "signed" : "unsigned") << "(X1) ";
		if(preAdderSubtracts) {
			vhdl << "-";
		} else {
			vhdl << "+";
		}
		vhdl << " " << (xIsSigned ? "signed" : "unsigned") << "(X2)); -- pre-adder" << endl;
	}

	int wIntermMult = wX + wY + onlyOneDelta;
	wM = IntMultiplier::prodsize(wX, wY, xIsSigned, yIsSigned);//(wX > 1 ? wX : 0) + (wY > 1 ? wY : 0) + (wX==1 && wY==1 ? 1 : 0); //consider special cases with wX or wY (or both) equals one
//	wM = wX + wY;
//	cout << "wM=" << wM << endl;
		
	vhdl << tab << declare(getTarget()->DSPMultiplierDelay(), "Mfull",wIntermMult) << " <= std_logic_vector(" << (signedMultOutput ? "signed" : "unsigned") << "("<<
		 (shouldPadX ? "'0' & " : "") <<"X) * " << (signedMultOutput ? "signed" : "unsigned") << "(" <<
		 (shouldPadY ? "'0' & " : "")<< "Y)); -- multiplier" << endl;

	vhdl << tab << declare("M", wM) << " <= Mfull" << range(wM - 1, 0) << ";" << endl;

	if(usePostAdder) {
		if(wZ > wM) THROWERROR("word size for input Z (which is " << wZ << " ) must be at most word size of multiplier result (which is " << wM << " ).");
		addInput("Z", wZ);
		vhdl << tab << declare(getTarget()->DSPAdderDelay(),"A",wM) << " <= std_logic_vector(" << (xIsSigned ? "signed" : "unsigned") << "(M) + " << (xIsSigned ? "signed" : "unsigned") << "(Z)); -- post-adder" << endl;
	}

	addOutput("R", wM);
	vhdl << tab << "R <= "<< (usePostAdder ? "A" : "M") << ";" << endl;
}

void DSPBlock::emulate(TestCase* tc)
{
	mpz_class svX = tc->getInputValue("X");
	mpz_class svY = tc->getInputValue("Y");
	mpz_class svR;

	if(xIsSigned_)
	{
		if (wX_ == 1) {
			svX = -svX;
		} else {
			// Manage signed digits
			mpz_class big1 = (mpz_class{1} << (wX_));
			mpz_class big1P = (mpz_class{1} << (wX_-1));

			if(svX >= big1P) {
				svX -= big1;
				//cerr << "X is neg. Interpreted value : " << svX.get_str(10) << endl;
			}
		}
	}
	if (yIsSigned_) {
		if (wY_ == 1) {
			svY = -svY;
		} else {
			mpz_class big2 = (mpz_class{1} << (wY_));
			mpz_class big2P = (mpz_class{1} << (wY_-1));

			if(svY >= big2P) {
				svY -= big2;
				//cerr << "Y is neg. Interpreted value : " << svY.get_str(10) << endl;
			}
		}
	}

	svY = svX * svY;
	tc->addExpectedOutput("R", svY);
}


/*
 ToDo: extend this to the DSP functionality (inc. pre- and post-adders


void DSPBlock::emulate (TestCase* tc)
{
	mpz_class svX = tc->getInputValue("X");
	mpz_class svY = tc->getInputValue("Y");
	mpz_class svR;

	svY = svX * svY;
	tc->addExpectedOutput("Y", svY);
}


void DSPBlock::buildStandardTestCases(TestCaseList* tcl)
{
	TestCase *tc;

	mpz_class x, y;

	// 1*1
	x = mpz_class(1);
	y = mpz_class(1);
	tc = new TestCase(this);
	tc->addInput("X", x);
	tc->addInput("Y", y);
	emulate(tc);
	tcl->add(tc);

	// -1 * -1
	x = (mpz_class(1) << wX_) -1;
	y = (mpz_class(1) << wY_) -1;
	tc = new TestCase(this);
	tc->addInput("X", x);
	tc->addInput("Y", y);
	emulate(tc);
	tcl->add(tc);

	// The product of the two max negative values overflows the signed multiplier
	x = mpz_class(1) << (wX_ -1);
	y = mpz_class(1) << (wY_ -1);
	tc = new TestCase(this);
	tc->addInput("X", x);
	tc->addInput("Y", y);
	emulate(tc);
	tcl->add(tc);
}
*/

OperatorPtr DSPBlock::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui)
{
	int wX,wY,wZ;
	bool usePostAdder, usePreAdder, preAdderSubtracts;
	bool isPipelined;
	bool xIsSigned,yIsSigned;
	ui.parseStrictlyPositiveInt(args, "wX", &wX);
	ui.parseStrictlyPositiveInt(args, "wY", &wY);
	ui.parsePositiveInt(args, "wZ", &wZ);
	ui.parseBoolean(args, "isPipelined", &isPipelined);
	ui.parseBoolean(args,"usePostAdder",&usePostAdder);
	ui.parseBoolean(args,"usePreAdder",&usePreAdder);
	ui.parseBoolean(args,"xIsSigned",&xIsSigned);
	ui.parseBoolean(args,"yIsSigned",&yIsSigned);
	ui.parseBoolean(args,"preAdderSubtracts",&preAdderSubtracts);

	return new DSPBlock(parentOp,target,wX,wY,xIsSigned,yIsSigned,isPipelined,wZ,usePostAdder,usePreAdder,preAdderSubtracts);
}

template <>
    const OperatorDescription<DSPBlock> op_descriptor<DSPBlock> {"DSPBlock", // name
						"Implements a DSP block commonly found in FPGAs incl. pre-adders and post-adders computing R = (X1+X2) * Y + Z",
						"BasicInteger", // categories
						"",
						"wX(int): size of input X (or X1 and X2 if pre-adders are used);\
						wY(int): size of input Y;\
						wZ(int)=0: size of input Z (if post-adder is used);\
						xIsSigned(bool)=0: input X is signed;\
						yIsSigned(bool)=0: input Y is signed;\
						isPipelined(bool)=1: every stage is pipelined when set to 1;\
						usePostAdder(bool)=0: use post-adders;\
						usePreAdder(bool)=0: use pre-adders;\
						preAdderSubtracts(bool)=0: if true, the pre-adder performs a pre-subtraction;",
					   ""};


}   //end namespace flopoco

