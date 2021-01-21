//
// Created by jk on 9/21/20.
//

/*
 *  TODO:
 *  Assert for valid method-string
 *  set lsbInt and lsbOut independently in user interface
 *
 *  Optimizations:
 *  Word sizes in feedbackpath have conservative computation. Using wcpg can optimze the wordsizes - the more coeffs the more potential
 *   */

#include "FixIIRShiftAdd.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

#if HAVE_WCPG
extern "C"
{
#include "wcpg.h"
}
#endif

// this sign-macro is introduced because the initial computation ignored the sign bit resp. only works with positive values
// so e.g 576 is okay with 10 bits, but since it is converted into SIGNED integer, we need one bit more
// Later implementation may not use singed values, so it can be easily set to zero using this macro
#define TEMP_SIGN_FOR_RESIZE 1

using namespace std;

// test with
// flopoco FixIIRShiftAdd msbIn=16 lsbIn=-8 lsbOut=-8 shifta=3 shiftb=0 coeffb="576:512:128:7:8" coeffa="2:4:1:7" guardBits=2 verbose=2 testbench n=0
// make -j4 && ./flopoco FixIIRShiftAdd msbIn=4 lsbIn=-4 lsbOut=-4 shifta=3 shiftb=2 coeffb="7:15:7:7:15" coeffa="2:4:1:7" guardBits=0 verbose=0 grapha="{{'R',[1],1,[1],0},{'A',[7],1,[1],0,3,[-1],0,0},{'O',[1],1,[1],1,0},{'O',[2],1,[1],1,1},{'O',[4],1,[1],1,2},{'O',[7],1,[7],1,0}}" graphb="{{'A',[7],1,[1],0,3,[-1],0,0},{'A',[15],1,[1],0,4,[-1],0,0},{'O',[7],1,[7],1,0},{'O',[15],1,[15],1,0}}" method="multiplierless" testbench n=0
// is coeffa 1/2:1/4 bc shifta = 3 (scalea = 1/(2^3) = 0.125)

namespace flopoco {
    FixIIRShiftAdd::FixIIRShiftAdd(OperatorPtr parentOp, Target* target, int lsbIn_, int lsbOut_, int msbIn_, int guardBits_, vector<string> coeffb_, vector<string> coeffa_, int64_t shifta_, int64_t shiftb_, string method_, string grapha_, string graphb_) :
            Operator(parentOp, target), lsbIn(lsbIn_), lsbOut(lsbOut_), msbIn(msbIn_), guardBits(guardBits_), coeffb(coeffb_), coeffa(coeffa_), shifta(shifta_), shiftb(shiftb_), method(method_), grapha(grapha_), graphb(graphb_) {
        srcFileName = "FixIIRShiftAdd";
        setCopyrightString("Jonas Kuehle (2020)");

        useNumericStd();
        useNumericStd_Signed();
        useNumericStd_Unsigned();

        ostringstream name;
        name << "FixIIRShiftAdd";
        setNameWithFreqAndUID(name.str());

        m = coeffa.size();
        n = coeffb.size();

        coeffa_d = (double *) malloc(m * sizeof(double));
        coeffa_si = (int64_t *) malloc(m * sizeof(int64_t));
        coeffb_d = (double *) malloc(n * sizeof(double));
        coeffb_si = (int64_t *) malloc(n * sizeof(int64_t));
        coeffa_mp_f = (mpfr_t *) malloc(m * sizeof(mpfr_t));
        coeffb_mp_f = (mpfr_t *) malloc(n * sizeof(mpfr_t));
        registerForwardTB_mpz = (mpz_t*)malloc(sizeof(mpz_t) * n);
        registerFeedbackTB_mpz = (mpz_t*)malloc(sizeof(mpz_t) * m);

        for (uint32_t i = 0; i < n; i++) {
            // parse the coeffs from the string, with Sollya parsing
            sollya_obj_t node;

            node = sollya_lib_parse_string(coeffb[i].c_str());
            if (node == 0)                    // If conversion did not succeed (i.e. parse error)
            THROWERROR("Unable to parse string " << coeffb[i] << " as a numeric constant");

            mpfr_init2(coeffb_mp_f[i], 10000);
            sollya_lib_get_constant(coeffb_mp_f[i], node);
            //mpz_t test;
            //sollya_lib_get_constant_as_mpz(test, node);
            coeffb_d[i] = mpfr_get_d(coeffb_mp_f[i], GMP_RNDN);
            coeffb_si[i] = mpfr_get_si(coeffb_mp_f[i], GMP_RNDN);

            REPORT(DETAILED, "b[" << i << "]=" << setprecision(15) << scientific << coeffb_d[i]);
        }

        for (uint32_t i = 0; i < m; i++) {
            // parse the coeffs from the string, with Sollya parsing
            sollya_obj_t node;

            node = sollya_lib_parse_string(coeffa[i].c_str());
            if (node == 0)                    // If conversion did not succeed (i.e. parse error)
            THROWERROR("Unable to parse string " << coeffb[i] << " as a numeric constant");

            mpfr_init2(coeffa_mp_f[i], 10000);
            sollya_lib_get_constant(coeffa_mp_f[i], node);
            coeffa_d[i] = mpfr_get_d(coeffa_mp_f[i], GMP_RNDN);
            coeffa_si[i] = mpfr_get_si(coeffa_mp_f[i], GMP_RNDN);
            REPORT(DETAILED, "a[" << i << "]=" << scientific << setprecision(15) << coeffa_d[i]);
        }
        wcpg = 0;
        wcpgBitGain = 0;

#if HAVE_WCPG

        REPORT(INFO, "Computing worst-case peak gain");
        coeffa_d_wcpg = (double *) malloc(m * sizeof(double));
        coeffb_d_wcpg = (double *) malloc(m * sizeof(double));
        for(int i = 0; i < m; i++)
        {
            coeffa_d_wcpg[i] = coeffa_d[i] * (1 / (pow(2, shifta)));
            REPORT(DETAILED, "coeffa_d_wcpg: " << coeffa_d_wcpg[i] << ", coeffa: " << coeffa_d[i] << ", shifta: " << shifta)
        }
        for(int i = 0; i < n; i++)
            coeffb_d_wcpg[i] = coeffb_d[i]*(1/(pow(2, shiftb)));

        if (!WCPG_tf(&wcpg, coeffb_d_wcpg, coeffa_d_wcpg, n, m, (int)0))
        THROWERROR("Could not compute WCPG");
        wcpgBitGain = ceil(log2(wcpg+1));
        REPORT(INFO, "Computed filter worst-case peak gain: wcpg=" << wcpg << ", wcpg bit gain: " << wcpgBitGain);

        double Heps;
        double one_d[1] = {1.0};
        if (!WCPG_tf(&Heps, one_d, coeffa_d_wcpg, 1, m, (int)0))
        THROWERROR("Could not compute WCPG");
        REPORT(INFO, "Computed error amplification worst-case peak gain: Heps=" << Heps);

#else
        THROWERROR("WCPG was not found (see cmake output), cannot compute worst-case peak gain H. Either provide H, or compile FloPoCo with WCPG");
#endif
        wIn = msbIn - lsbIn + 1;
        msbInt = ceil(msbIn + wcpgBitGain);
        msbOut = msbInt;
        lsbInt = lsbOut;
        int wOut = msbOut - lsbOut + 1;
        //int wInt = wOut + guardBits; // not needed yet
        REPORT(DETAILED, "msbIn: \t\t\t\t" << msbIn << ", msbIn: \t\t\t\t " << msbIn)
        REPORT(DETAILED, "msbInt: \t\t\t\t" << msbInt << ", lsbInt: \t\t\t\t" << lsbInt)

        // ################################### WORD SIZES FORWARD PATH ############################################

        msbRegisterForwardOut = (int *) malloc(n * sizeof(int));
        msbAdditionsForwardOut = (int *) malloc(n * sizeof(int));
        msbMultiplicationsForwardOut = (int *) malloc(n * sizeof(int));
        lsbForwardPath = lsbIn; // lsb is constant in forward path until scaling

        // for n coefficients there are n multiplication, but n-1 registers and additions

        // multiplications
        for(int i = 0; i < n; i++)
        {
            msbMultiplicationsForwardOut[i] = msbIn + ceil(log2(abs(coeffb_si[i])+1));
            REPORT(DETAILED, "msbMultiplicationsForwardOut[i]: " << msbMultiplicationsForwardOut[i])
        }

        // registers (only word size is needed for vhdl code generation, but it can be derived from msb and lsb)
        for(int i = n-2; i >= 0; i--)
        {
            int bitGainForRegister = 0;
            for (int j = i + 1; j < n; j++) {
                bitGainForRegister += abs(coeffb_si[j]);
            }
            msbRegisterForwardOut[i] = msbIn + ceil(log2(abs(bitGainForRegister) + 1));
            REPORT(DETAILED, "msbRegisterForwardOut["<<i<<"]: " << msbRegisterForwardOut[i])
        }

        // additions, last addition (addN-1) is special case since it has two inputs
        // assert necessary for implementation reasons
        // It would be possible to copy msb from preceding register
        if (n < 2)
        THROWERROR("Number of coefficients in forward path (coeffb) must be larger or equal two");
        msbAdditionsForwardOut[n - 1] = msbIn + ceil(log2(abs(coeffb_si[n - 1]) + abs(coeffb_si[n - 2]) + 1));
        REPORT(DETAILED, "msbAdditionsForwardOut["<<n - 1<<"]: " << msbAdditionsForwardOut[n-1])

        for (int i = n - 2; i >= 0; i--) // for additions except addN-1
        {
            int bitGainForAdditons = 0;
            for (int j = i; j < n; j++)
                bitGainForAdditons += abs(coeffb_si[j]);
            msbAdditionsForwardOut[i] = msbIn + ceil(log2(abs(bitGainForAdditons) + 1));
            REPORT(DETAILED, "msbAdditionsForwardOut["<<i<<"]: " << msbAdditionsForwardOut[i])
        }

        // scaleb
        msbScaleBOut = msbAdditionsForwardOut[0] - abs(shiftb);
        lsbScaleBOut = lsbForwardPath - abs(shiftb);
        REPORT(DETAILED, "msbScaleBOut: \t\t\t" << msbScaleBOut << ", lsbScaleBOut: \t\t\t" << lsbScaleBOut)

        // ################################### WORD SIZES BACKPATH ###############################################

        // check if replaceble by wcpg
        int wordsizeGainFeedback = 0; // growth of wordsize in feedback path. Used multiple times for wird size / msb / lsb calculation
        for (int i = 0; i < m; i++)
            wordsizeGainFeedback += (abs(coeffa_si[i]));
        wordsizeGainFeedback = ceil(log2(wordsizeGainFeedback + 1));
        REPORT(DETAILED, "wordsizeGainFeedback: " << wordsizeGainFeedback);

        msbRegisterFeedbackOut = (int *) malloc(m * sizeof(int));
        lsbRegisterFeedbackOut = (int *) malloc(m * sizeof(int));
        msbMultiplicationsFeedbackOut = (int *) malloc(m * sizeof(int));
        lsbMultiplicationsFeedbackOut = (int *) malloc(m * sizeof(int));
        msbAdditionsFeedbackOut = (int *) malloc(m * sizeof(int));
        lsbAdditionsFeedbackOut = (int *) malloc(m * sizeof(int));
        lsbInt -= guardBits;
        REPORT(DETAILED, "\t\t\t\t\tlsbInt incl. guardBits: \t\t" << lsbInt)

        // Multiplications
        for(int i = 0; i < m; i++)
        {
            msbMultiplicationsFeedbackOut[i] = msbInt + ceil(log2(abs(coeffa_si[i])+1));
            lsbMultiplicationsFeedbackOut[i] = lsbInt;
            REPORT(DETAILED, "msbMultiplicationsFeedbackOut["<<i<<"]: \t" << msbMultiplicationsFeedbackOut[i] << ", lsbMultiplicationsFeedbackOut["<<i<<"] \t" << lsbMultiplicationsFeedbackOut[i])
        }

        // registers
        // values of registers are computed first -> values for additions can be derived
        for(int i = m-1; i >= 0; i--)
        {
            int tmp = 0;
            for (int j = i; j < m; j++)
                tmp += coeffa_si[j];
            msbRegisterFeedbackOut[i] = msbInt+ceil(log2(abs(tmp)+1));
            lsbRegisterFeedbackOut[i] = lsbInt;
            REPORT(DETAILED, "msbRegisterFeedbackOut["<<i<<"]: \t" << msbRegisterFeedbackOut[i] << ", lsbRegisterFeedbackOut["<<i<<"]: \t\t" << lsbRegisterFeedbackOut[i])
        }

        // Additions, first addition is special case, computation depends on register size
        // Output from an addition is equal to the input of the succeeding register and registers values are already computed
        // exception for add0, see comment for addition 0 below
        for(int i = m-1; i > 0; i--)
        {
            msbAdditionsFeedbackOut[i] = msbRegisterFeedbackOut[i-1];
            lsbAdditionsFeedbackOut[i] = lsbInt;
            REPORT(DETAILED, "msbAdditionsFeedbackOut["<<i<<"]: \t" << msbAdditionsFeedbackOut[i] << ", lsbAdditionsFeedbackOut["<<i<<"]: \t" << lsbAdditionsFeedbackOut[i])
        }

        /*
         * Addition 0
         * The computation for the msb and word sizes in the forward and feedback path is very conservative.
         * This may lead to the fact, that one input word size of the adder is larger than the result
         * The maximum word size is given by msbIn + wcpgBitGain, so the maximum output of the adder is
         * msbIn + wcpgBitGain + shifta since it is the latest operation (msbInt = msbIn + wcpgBitGain)
         */

        msbAdditionsFeedbackOut[0] = max(msbScaleBOut, msbInt + shifta);
        lsbAdditionsFeedbackOut[0] = min(lsbInt, lsbScaleBOut);
        REPORT(DETAILED, "msbAdditionsFeedbackOut[0]: \t" << msbAdditionsFeedbackOut[0] << ", lsbAdditionsFeedbackOut[0]: \t" << lsbAdditionsFeedbackOut[0])

        // ScaleA, only shift fixed point, no code generation
        msbScaleAOut = msbAdditionsFeedbackOut[0]-(int)shifta;
        lsbScaleAOut = lsbAdditionsFeedbackOut[0]-(int)shifta;
        REPORT(DETAILED, "msbScaleAOut: \t\t\t" << msbScaleAOut << ", lsbScaleAOut: \t\t\t" << lsbScaleAOut)

        // truncationAfterScaleA
        msbTruncationIntOut = msbScaleAOut;
        lsbTruncationIntOut = lsbInt;
        REPORT(DETAILED, "msbTruncationIntOut: \t\t" << msbTruncationIntOut << ", lsbTruncationIntOut: \t\t" << lsbTruncationIntOut)

        addInput("X", wIn, true);
        addOutput("Result", wOut);
        REPORT(DETAILED, "input size (wIn): " << wIn)
        REPORT(DETAILED, "output size (wOut): " << wOut)

        // ################################# TESTBENCH INIT #########################################

        // In VHDL there are for n coefficients n-1 registers, but in testbench register0 stores the input x
        // so there are n registers for n coefficients. So register0 has word length of msbIn msbIn - lsbIn +1

        //mpz_init2(registerForwardTB_mpz[0] , msbIn - lsbIn + 1);
        mpz_init(registerForwardTB_mpz[0]);
        mpz_set_si(registerForwardTB_mpz[0], 0);

        for(int i = 1; i < n; i++)
        {
            //mpz_init2(registerForwardTB_mpz[i], msbRegisterForwardOut[i-1] - lsbForwardPath + 1); // mpz_reg[i] has msb of reg[i-1] since reg0 in testbench is emulated as input
            mpz_init(registerForwardTB_mpz[i]);
            mpz_set_si(registerForwardTB_mpz[i], 0); // necessary of 0 by default?
        }


        for(int i = 0; i < m; i++)
        {
            //mpz_init2(registerFeedbackTB_mpz[i], msbRegisterFeedbackOut[i] - lsbRegisterFeedbackOut[i] + 1 + guardBits);
            mpz_init(registerFeedbackTB_mpz[i]);
            mpz_set_si(registerFeedbackTB_mpz[0], 0); // necessary of 0 by default?
        }

        // ################################# CODE GENERATION FORWARD PATH #########################################
        /*
         *
         *  Multiplications
         *  TEMP_SIGN_FOR_RESIZE adds one dedicated bit for the resize operation. It is necessary, since the word size
         *  computation is made for positive integers. Additional space for the sign bit can be obtained by
         *  increasing the msbIn by 1. Since VHDL computed the resulting word size of a multiplication by adding the
         *  word sizes of both factors, additional scaling at the end of the line is necessary to resize the word to the
         *  originated word size (-1 there is mandatory)
         */

        if(method == "plain")
        {
            // multiplications
            for (int i = 0; i < n; i++) {
                vhdl << declare(join("forMul", i), ceil(wIn + log2(abs(coeffb_si[i]) + 1)))
                     << " <= std_logic_vector(signed(X) * (signed(std_logic_vector(to_signed(" << coeffb_si[i] << ","
                     << ceil(log2(abs(coeffb_si[i]) + 1)) + TEMP_SIGN_FOR_RESIZE << ")))))("
                     << ceil(wIn + log2(abs(coeffb_si[i]) + 1) - 1) << " downto 0);" << endl;
            }
        }

        else if (method=="multiplierless")
        {
            stringstream outPortMaps;
            for (int i = 0; i < n; i++)
            {
                if (FixIIRShiftAdd::getIndexCoeff(coeffb_si, n, coeffb_si[i]) == i) // first time occurence of element, if the result if getIndexCoeff() is not i it means that the element was found earlier in the array, hence duplicate
                {
                    if(i != 0) // avoid heading comma
                        outPortMaps << ",";

                    declare(join("forMul", i), msbMultiplicationsForwardOut[i] - lsbForwardPath +1);
                    // check for negative values
                    if(coeffb_si[i] >= 0)
                        outPortMaps << "R_c" << coeffb_si[i] << "=>forMul" << i;
                    else // if value is negative
                        outPortMaps << "R_cm" << abs(coeffb_si[i]) << "=>forMul" << i; // replace the '-' with 'm'
                }
                else // duplicate detected, set the duplicate to the signal where it was first found, so if coeff_b[0] and coeff_b[2] are equal, forMul2 is set to forMul0
                {
                    REPORT(LIST, "duplicate detected (" << coeffb_si[i] << ")")
                    vhdl << declare(join("forMul", i), msbMultiplicationsForwardOut[i] - lsbForwardPath +1) << "<= std_logic_vector(forMul" << FixIIRShiftAdd::getIndexCoeff(coeffb_si, n, coeffb_si[i]) << ");" <<  endl;
                }
            }

            stringstream parameters;
            parameters << "wIn=" << wIn << " graph=" << graphb;
            string inPortMaps = "X0=>X";
            REPORT(LIST, "outPortMaps (forward): " << outPortMaps.str())
            newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponentForward", parameters.str(), inPortMaps,
                        outPortMaps.str());

            REPORT(DETAILED, "IntConstMultShiftAdd for forward path created")
        }

        vhdl << endl;
        // additions
        for (int i = 0; i < n - 1; i++) {
            vhdl << declare(join("forAdd", i), msbAdditionsForwardOut[i] - lsbForwardPath + 1) << " <= std_logic_vector(resize(signed("
                 << join("forRegOut", i) << "), " << msbAdditionsForwardOut[i] - lsbForwardPath + 1 << ") + signed(" << join("forMul", i) << "));"
                 << endl;
        }
        vhdl << endl;
        // registers
        for (int i = 0; i < n - 2; i++) {
            newInstance("ShiftReg", join("forReg", i), join("n=1 reset=2 w=", msbRegisterForwardOut[i] - lsbForwardPath + 1),
                        join("X=>forAdd", i + 1), join("Xd1=>forRegOut", i));
        }
        // treat special case for regN-1 since it has different input (from mult not from add)
        newInstance("ShiftReg", join("forReg", n - 2), join("n=1 reset=2 w=", msbRegisterForwardOut[n-2] - lsbForwardPath + 1),
                    join("X=>forMul", n - 1), join("Xd1=>forRegOut", n - 2));
        vhdl << endl;
        // ################################# CODE GENERATION BACKPATH #########################################

        // truncation, truncate the least significant bits
        // the amount of truncated bits is lsbInt - lsbScaleAOut, e.g. -8-(-12) = 4, which results in lsbInt bits
        vhdl << declare("truncationAfterScaleA", msbInt - lsbInt + 1) << " <= feedbackAdd0("<< (msbScaleAOut - lsbScaleAOut) << " downto " << lsbInt - lsbScaleAOut << ");" << endl;

        // multiplications, see comment multiplications forward path for explanation of TEMP_SIGN_FOR_RESIZE
        if (method=="plain")
        {
        for (int i = 0; i < m; i++)
            {
                vhdl << declare(join("feedbackMul", i), msbMultiplicationsFeedbackOut[i] - lsbMultiplicationsFeedbackOut[i] + 1)
                     << " <= std_logic_vector(resize(signed(truncationAfterScaleA), " << msbInt - lsbInt + 1 << " ) * (signed(std_logic_vector(to_signed(" << -coeffa_si[i] << ","
                     << ceil(abs(log2((coeffa_si[i] + 1)))) + TEMP_SIGN_FOR_RESIZE << ")))))(" << msbMultiplicationsFeedbackOut[i] - lsbMultiplicationsFeedbackOut[i] + 1 - 1 << " downto 0);" << endl;
            }
        }

        else if (method=="multiplierless")
        {
            stringstream outPortMaps;
            for (int i = 0; i < m; i++)
            {
                if (FixIIRShiftAdd::getIndexCoeff(coeffa_si, m, coeffa_si[i]) == i) // first time occurence of element, if the result if getIndexCoeff() is not i it means that the element was found earlier in the array, hence duplicate
                {
                    if(i != 0) // avoid heading comma
                        outPortMaps << ",";

                    declare(join("feedbackMul", i), msbMultiplicationsFeedbackOut[i] - lsbMultiplicationsFeedbackOut[i] +1);
                    // check for negative values
                    if(coeffa_si[i] >= 0)
                        outPortMaps << "R_c" << coeffa_si[i] << "=>feedbackMul" << i;
                    else // if value is negative
                        outPortMaps << "R_cm" << abs(coeffa_si[i]) << "=>feedbackMul" << i; // replace the '-' with 'm'
                }
                else // duplicate detected, set the duplicate to the signal where it was first found, so if coeff_b[0] and coeff_b[2] are equal, feedbackMul is set to feedbackMul
                {
                    REPORT(LIST, "duplicate detected (" << coeffa_si[i] << ")")
                    vhdl << declare(join("feedbackMul", i), msbMultiplicationsFeedbackOut[i] - lsbMultiplicationsFeedbackOut[i] +1) << "<= std_logic_vector(feedbackMul" << FixIIRShiftAdd::getIndexCoeff(coeffa_si, m, coeffa_si[i]) << ");" <<  endl;
                }
            }

            stringstream parameters;
            parameters << "wIn=" << msbTruncationIntOut-lsbTruncationIntOut+1 << " graph=" << grapha;
            string inPortMaps = "X0=>truncationAfterScaleA";
            REPORT(LIST, "outPortMaps (feedback): " << outPortMaps.str())
            newInstance("IntConstMultShiftAdd", "IntConstMultShiftAddComponentFeedback", parameters.str(), inPortMaps,
                        outPortMaps.str());

            REPORT(DETAILED, "IntConstMultShiftAdd for feedback path created")
        }

        /*
         * Additions
         * addition0 ist special case since it has scaleB as input and the other input is possible too large bc of the
         * conservative computation in the feedback path (see comment msb computation for this addition)
         * --> feedbackRegOut0 has to be rescaled to max output of adder (later it can be optimized to mInt + wcpgGain - (msbScaleBOut - msbIn) + shifta
         * both inputs of the additions must have same amount of lsb-bits
         */

        string operatorFeedbackpathMethod;
        if(method == "plain")
            operatorFeedbackpathMethod = "+";
        if(method == "multiplierless")
            operatorFeedbackpathMethod = "+";

        int numLsbBitsDifference = abs(lsbRegisterFeedbackOut[0] - lsbScaleBOut);
        REPORT(DETAILED, "numLsbBitsDifference: " << numLsbBitsDifference)
        if(lsbScaleBOut < lsbRegisterFeedbackOut[0])
        {
            // lsbScaleBOut has more lsb bits, so add lsb-bits to regOut0
            // resize and shifting (to the left), scale this value to the size of the output of feedbackAdd0, since this value cannot be larger
            // the word size cannot be larger than the word size of the adder (see comment msb computation for this addition)
            wordsizefeedbackAdd0ResizedReg0 = min(msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1, msbRegisterFeedbackOut[0] - lsbRegisterFeedbackOut[0] + 1 + numLsbBitsDifference);
            vhdl << declare("feedbackAdd0ResizedReg0", wordsizefeedbackAdd0ResizedReg0) << " <= std_logic_vector(shift_left(resize(signed(feedbackRegOut0), " << wordsizefeedbackAdd0ResizedReg0 << "),"<< numLsbBitsDifference <<"));" << endl;
            vhdl << declare("feedbackAdd0", msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1) << " <= std_logic_vector(resize(signed(forAdd0), " << msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1 << ") "<< operatorFeedbackpathMethod <<" signed(feedbackAdd0ResizedReg0));" << endl;
        }

        else if(lsbScaleBOut > lsbRegisterFeedbackOut[0])
        {
            //vhdl << declare("feedbackAdd0ResizedScaleB", msbScaleBOut - lsbScaleBOut + numLsbBitsDifference + 1) << " <= std_logic_vector(shift_left(signed(scaleBOut(" << msbScaleBOut - lsbScaleBOut +1  << " downto 0))), to_integer(to_signed(1, 2)));" << endl;
            wordsizefeedbackAdd0ResizedScaleB = min(msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1, msbScaleBOut - lsbScaleBOut + 1 + numLsbBitsDifference);
            vhdl << declare("feedbackAdd0ResizedScaleB", wordsizefeedbackAdd0ResizedScaleB) << " <= std_logic_vector(shift_left(resize(signed(forAdd0), " << wordsizefeedbackAdd0ResizedScaleB << "),"<< numLsbBitsDifference <<"));" << endl;
            vhdl << declare("feedbackAdd0", msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1) << " <= std_logic_vector(resize(signed(feedbackAdd0ResizedScaleB), " << msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1 << ") "<< operatorFeedbackpathMethod <<" resize(signed(feedbackRegOut0), " << msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1 << "));" << endl;
        }

        else // if both inputs are well aligned, use non-resized inputs
        {
            vhdl << declare("feedbackAdd0", msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1) << " <= std_logic_vector(resize(signed(forAdd0), " << msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1 << ") "<< operatorFeedbackpathMethod <<" resize(signed(feedbackRegOut0), " << msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1 << "));" << endl;
        }

        for (int i = 1; i < m; i++)
        {
            vhdl << declare(join("feedbackAdd", i), msbAdditionsFeedbackOut[i] - lsbAdditionsFeedbackOut[i] + 1) << " <= std_logic_vector(resize(signed("
                 << join("feedbackRegOut", i) << "), " << msbAdditionsFeedbackOut[i] - lsbAdditionsFeedbackOut[i] + 1 << ") "<< operatorFeedbackpathMethod <<" signed(" << join("feedbackMul", i-1) << "));"
                 << endl;
        }
/*
        // registers
        // special case for regm-1
        vhdl << declare(join("feedbackMulNeg", m - 1), msbMultiplicationsFeedbackOut[m-1]-lsbMultiplicationsFeedbackOut[m-1]+1) << " <= std_logic_vector(resize(signed(feedbackMul3) * (to_integer(to_signed(-1,2))),21));" << endl;
        newInstance("ShiftReg", join("feedbackReg", m - 1), join("n=1 reset=1 w=", msbRegisterFeedbackOut[m - 1] - lsbRegisterFeedbackOut[m-1] + 1),
                    join("X=>feedbackMulNeg", m - 1), join("Xd1=>feedbackRegOut", m - 1));
*/
        newInstance("ShiftReg", join("feedbackReg", m - 1), join("n=1 reset=1 w=", msbRegisterFeedbackOut[m - 1] - lsbRegisterFeedbackOut[m-1] + 1),
                   join("X=>feedbackMul", m - 1), join("Xd1=>feedbackRegOut", m - 1));

        for (int i = 0; i < m - 1; i++) {
            newInstance("ShiftReg", join("feedbackReg", i), join("n=1 reset=1 w=", msbRegisterFeedbackOut[i] - lsbRegisterFeedbackOut[i] + 1),
                        join("X=>feedbackAdd", i + 1), join("Xd1=>feedbackRegOut", i));
        }

        // shift feedback0 to the right, resp. cut two more bits, guard bits also considered
        vhdl << declare("endresult", msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0] + 1 - abs(lsbAdditionsFeedbackOut[0]-lsbInt - guardBits)) << " <= feedbackAdd0("<< (msbAdditionsFeedbackOut[0] - lsbAdditionsFeedbackOut[0]) << " downto " << abs(lsbAdditionsFeedbackOut[0]-lsbInt) + guardBits << ");" << endl;
        vhdl << "Result <= std_logic_vector(resize(signed(endresult)," << wOut << "));" << endl;

    }

    FixIIRShiftAdd::~FixIIRShiftAdd()
    {
        delete(coeffb_d);
        delete(coeffa_d);
        delete(coeffa_si);
        delete(coeffb_si);
        delete(coeffa_d_wcpg);
        delete(coeffb_d_wcpg);
        delete(msbRegisterForwardOut);
        delete(msbAdditionsForwardOut);
        delete(msbMultiplicationsForwardOut);
        delete(msbRegisterFeedbackOut);
        delete(lsbRegisterFeedbackOut);
        delete(msbMultiplicationsFeedbackOut);
        delete(lsbMultiplicationsFeedbackOut);
        delete(msbAdditionsFeedbackOut);
        delete(lsbAdditionsFeedbackOut);

        mpz_clear(resultForwardPathTB_mpz);
        mpz_clear(feedbackAdd0);

        for (uint32_t i = 0; i < n; i++)
        {
            mpfr_clear(coeffb_mp_f[i]);
            mpz_clear(registerForwardTB_mpz[i]);
        }

        for (uint32_t i = 0; i < m; i++)
        {
            mpfr_clear(coeffa_mp_f[i]);
            mpz_clear(registerForwardTB_mpz[i]);
        }
    };

    void FixIIRShiftAdd::emulate(TestCase * tc)
    {
        mpz_class sx_ = tc->getInputValue("X"); 		    // get the input bit vector as an integer
        sx_ = bitVectorToSigned(sx_, msbIn-lsbIn+1); 	// convert it to a signed mpz_class
        mpz_t x_;
        //mpz_init2(x_, msbIn-lsbIn+1); // restrict size
        mpz_init(x_);
        mpz_set_si(x_, sx_.get_si()); // convert input to mpz_t
        //mpz_init2(resultForwardPathTB_mpz, msbScaleBOut - lsbScaleBOut + 1);
        mpz_init(resultForwardPathTB_mpz);
        mpz_t coeffResult;
        //mpz_init2(coeffResult, msbScaleBOut - lsbInt +1); // use this as word width since it cannot be larger than that, can be optimzed but not neccessary
        mpz_init(coeffResult);

        mpz_set_si(resultForwardPathTB_mpz, 0);
        mpz_set_si(registerForwardTB_mpz[0], mpz_get_si(x_));
        for(int i = 0; i < n; i++)
        {
            // no add_mul_si() for mpz_t
            mpz_mul_si(coeffResult, registerForwardTB_mpz[i], coeffb_si[i]); // coeffb_d[i] * registerforwardTB[i];
            mpz_add(resultForwardPathTB_mpz, resultForwardPathTB_mpz, coeffResult);
        }
        for(int i = n-1; i >= 1; i--)
        {
            mpz_set_si(registerForwardTB_mpz[i], mpz_get_si(registerForwardTB_mpz[i-1]));
        }

        // scaleB
        //mpz_fdiv_q_2exp(resultForwardPathTB_mpz, resultForwardPathTB_mpz, abs(shiftb)); // use fdiv, not tdiv
        //REPORT(LIST, "register forward out mpz: " << resultForwardPathTB_mpz << ", shifted: " << (mpz_get_si(resultForwardPathTB_mpz) >> abs(lsbIn)))

        // ######################################## TESTBENCH BACK PATH ###############################################

        mpz_init(feedbackAdd0);
        mpz_set_si(feedbackAdd0, 0);

        // align both inputs for addition feedbackadd0
        // input from forward path is affected by scaleb, input from feedbackpath is affected by scalea and guard bits
        //

        int numLsbBitsDifferenceTB = abs(lsbRegisterFeedbackOut[0] - lsbScaleBOut);
        REPORT(DETAILED, "numLsbBitsDifferenceTB: " << numLsbBitsDifferenceTB)
        if(lsbScaleBOut < lsbRegisterFeedbackOut[0])
        {
            // lsbScaleBOut has more lsb bits, so add lsb-bits to regOut0
            mpz_mul_2exp(registerFeedbackTB_mpz[0], registerFeedbackTB_mpz[0], numLsbBitsDifferenceTB);
        }

        else if(lsbScaleBOut > lsbRegisterFeedbackOut[0])
        {
            // output of forwardpath has more lsb, so add lsb bits to output of forward path (result_forwardpath)
            mpz_mul_2exp(resultForwardPathTB_mpz, resultForwardPathTB_mpz, numLsbBitsDifferenceTB);
        }

        // add 0
        mpz_add(feedbackAdd0, registerFeedbackTB_mpz[0], resultForwardPathTB_mpz);
        REPORT(DETAILED, "feedbackAdd0: " << feedbackAdd0 << ", registerFeedbackTB_mpz[0]: " << registerFeedbackTB_mpz[0] << ", resultForwardPathTB_mpz: " << resultForwardPathTB_mpz)

        // set result and shift to align to lsbOut since lsbAdditionsFeedbackOut[0] can different lsb value (only more lsb)
        mpz_t result_mpz;
        mpz_init_set(result_mpz, feedbackAdd0);
        mpz_fdiv_q_2exp(result_mpz, result_mpz, abs(lsbAdditionsFeedbackOut[0]-lsbInt)); // realign result
        mpz_fdiv_q_2exp(result_mpz, result_mpz, guardBits); // shift back bc of guard bits
        REPORT(DETAILED, "")
        REPORT(DETAILED, "RESULT: " << result_mpz << ", shifted: " << (mpz_get_si(result_mpz))/(int)pow(2,abs(lsbOut)))
        mpz_class result_class(result_mpz);
        tc->addExpectedOutput("Result", result_class);

        // scaling (only in code, msb and lsb is altered)
        // truncation
        mpz_t truncated;
        mpz_init(truncated);
        // current lsb is lsb from feedbackAdd0

        REPORT(DETAILED, "lsbAdditionsFeedbackOut: "<< lsbAdditionsFeedbackOut[0] << ", scaleAOut: " << lsbScaleAOut << ", lsbInt: " << lsbInt);
        // truncate to lsbInt
        REPORT(DETAILED, "truncate " << feedbackAdd0 << "by " << abs(lsbScaleAOut-lsbInt) << " bits")
        mpz_fdiv_q_2exp(truncated, feedbackAdd0, abs(lsbScaleAOut-lsbInt)); // dont use tdiv, use fdiv!

        REPORT(DETAILED, "truncated: " << truncated)
        REPORT(DETAILED, "add0: " << feedbackAdd0)
        for(int i = 0; i < m; i++)
        REPORT(DETAILED,"register" << i << ": " << registerFeedbackTB_mpz[i])

        for(int i = 0; i < m; i++)
        {
            mpz_t coeffR;
            mpz_init(coeffR);
            mpz_mul_si(coeffR, truncated, coeffa_si[i]);
            // mpz_mul_si(coeffR, coeffR, -1);
            REPORT(DETAILED, "coeffr mul" << i << ": " << mpz_get_si(coeffR))
            mpz_sub(registerFeedbackTB_mpz[i], registerFeedbackTB_mpz[i+1], coeffR);
        }
    }

    void FixIIRShiftAdd::buildStandardTestCases(TestCaseList* tcl)
    {
        int largestPossibleValue = pow(2, msbIn - lsbIn +1)-1;
        int numTestcases = 10;

        int* testcases = (int*) malloc(numTestcases*sizeof(int));
        for(int i = 0; i < numTestcases; i++)
        {
            testcases[i] = 0;
        }
        testcases[0] = 16;

        TestCase *tc;
        for(int i = 0; i < numTestcases; i++)
        {
            tc = new TestCase(this);
            tc->addInput("X", mpz_class(testcases[i]));
            emulate(tc);
            tcl->add(tc);
        }
    }

    OperatorPtr FixIIRShiftAdd::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args) {
        int msbIn;
        UserInterface::parseInt(args, "msbIn", &msbIn);
        int lsbIn;
        UserInterface::parseInt(args, "lsbIn", &lsbIn);
        int lsbOut;
        UserInterface::parseInt(args, "lsbOut", &lsbOut);

        int shifta;
        UserInterface::parseInt(args, "shifta", &shifta);
        int shiftb;
        UserInterface::parseInt(args, "shiftb", &shiftb);

        int guardBits;
        UserInterface::parseInt(args, "guardbits", &guardBits);

        vector<string> coeffb;
        vector<string> coeffa;

        string in;
        UserInterface::parseString(args, "coeffb", &in);
        stringstream ssa(in);
        while( ssa.good() )	{
            string substr;
            getline( ssa, substr, ':' );
            coeffb.push_back( substr );
        }

        UserInterface::parseString(args, "coeffa", &in);
        stringstream ssb(in);
        while( ssb.good() )	{
            string substr;
            getline( ssb, substr, ':' );
            coeffa.push_back( substr );
        }

        string method;
        UserInterface::parseString(args, "method", &method);

        string grapha;
        UserInterface::parseString(args, "grapha", &grapha);

        string graphb;
        UserInterface::parseString(args, "graphb", &graphb);

        return new FixIIRShiftAdd(parentOp, target, lsbIn, lsbOut, msbIn, guardBits, coeffb, coeffa, shifta, shiftb, method, grapha, graphb);
    }

    TestList FixIIRShiftAdd::unitTest(int index)
    {
        TestList testStateList;
        vector<pair<string,string>> paramList;

        paramList.push_back(make_pair("msbIn",  "21"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "576:512:128:7:8"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "21"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "21"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "-576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "21"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "-576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        paramList.push_back(make_pair("guardbits",  "1"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "21"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "-576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        paramList.push_back(make_pair("guardbits",  "5"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "17"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "-576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        paramList.push_back(make_pair("guardbits",  "1"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "17"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-10"));
        paramList.push_back(make_pair("coeffb",  "-576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        paramList.push_back(make_pair("guardbits",  "1"));
        testStateList.push_back(paramList);
        paramList.clear();

        paramList.push_back(make_pair("msbIn",  "17"));
        paramList.push_back(make_pair("lsbIn",  "-6"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "-576:512:128:7:8:5:6"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7:2:2"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("shiftb",  "0"));
        paramList.push_back(make_pair("guardbits",  "1"));
        testStateList.push_back(paramList);
        paramList.clear();
/*
        // multiplierless
        paramList.push_back(make_pair("msbIn",  "21"));
        paramList.push_back(make_pair("lsbIn",  "-8"));
        paramList.push_back(make_pair("lsbOut", "-8"));
        paramList.push_back(make_pair("coeffb",  "7:15:7:-7:15"));
        paramList.push_back(make_pair("coeffa",  "2:4:1:7"));
        paramList.push_back(make_pair("shifta",  "3"));
        paramList.push_back(make_pair("grapha",  "{{'A',[7],1,[1],0,3,[-1],0,0},{'A',[15],1,[1],0,4,[-1],0,0},{'O',[-7],1,[-7],1,0},{'O',[7],1,[7],1,0},{'O',[15],1,[15],1,0}}"));
        paramList.push_back(make_pair("grapha",  "{{'A',[7],1,[1],0,3,[-1],0,0},{'A',[15],1,[1],0,4,[-1],0,0},{'O',[7],1,[7],1,0},{'O',[15],1,[15],1,0}}"));
        paramList.push_back(make_pair("method",  "multiplierless"));
        paramList.push_back(make_pair("shiftb",  "0"));
        testStateList.push_back(paramList);
        paramList.clear();
*/
        return testStateList;
    }

    void FixIIRShiftAdd::registerFactory(){
        UserInterface::add("FixIIRShiftAdd",
                           "An Infinite Impulse Response filter generator using IntConstMultShiftAdd (optional).",
                           "FiltersEtc", // categories
                           "",
                           "msbIn(int): input most significant bit;\
                        lsbIn(int): input least significant bit;\
                        lsbOut(int): output least significant bit;\
                        guardbits(int)=0: number of guard bits for computation in recursive feedback path;\
                        coeffa(string): colon-separated list of real coefficients using Sollya syntax. Example: coeffa=\"1.234567890123:sin(3*pi/8)\";\
                        coeffb(string): colon-separated list of real coefficients using Sollya syntax. Example: coeffb=\"1.234567890123:sin(3*pi/8)\";\
                        shifta(int): Num of rightshifts for coeffa (must be positive);\
                        shiftb(int): Num of rightshifts for coeffb (must be positive);\
                        method(string)=plain: plain or multiplierless;\
                        grapha(string)=emptya: graph in rpag format for coeffa;\
                        graphb(string)=emptyb: graph in rpag format for coeffb;\
                        ",
                           "",
                           FixIIRShiftAdd::parseArguments,
                           FixIIRShiftAdd::unitTest
        ) ;
    }

    int64_t FixIIRShiftAdd::getIndexCoeff(int64_t* coeff, int64_t arrayLength, int64_t val) // TODO: rename
    {
        for(int i = 0; i < arrayLength; i++)
        {
            if(coeff[i] == val)
                return i;
        }
        return -1;
    }

}