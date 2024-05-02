/*

  This file is part of the FloPoCo project
  initiated by the Aric team at Ecole Normale Superieure de Lyon
  and developed by the Socrate team at Institut National des Sciences Appliquées de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA,
  2008-2014.
  All rights reserved.

*/
#include "flopoco/FixFunctions/SNAFU.hpp"

#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/FixFunctions/FixFunctionEmulator.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>

using namespace std;


#define LARGE_PREC 1000  // 1000 bits should be enough for everybody

// Mux definition for the ReLU
static inline const string relu(int wIn, int wOut, bool derivative = false)
{
  std::ostringstream s;
  s << zg(wOut) << " when X" << of(wIn - 1) << " = '1' else '0' & ";

  if(derivative) {
    s << og(wIn - 1);
  } else {
    s << "X" << range(wIn - 2, 0);
  }

  s << ";" << endl;

  return s.str();
}

static inline string _replace(string s, const string x, const string r)
{
  size_t pos;
  while((pos = s.find(x)) != string::npos) {
    s.replace(pos, 1, r);
  }
  return s;
}

// Replace X with r, assuming future Xs are symbolized by @ in r
static inline string replaceX(string s, const string r)
{
  return _replace(_replace(s, "X", r), "@", "X");
}

static inline string input(size_t n)
{
  return "X" + to_string(n);
}

static inline string output(size_t n)
{
  return "Y" + to_string(n);
}

namespace flopoco
{
  SNAFU::SNAFU(OperatorPtr parentOp_,
    Target* target_,
    string fIn,
    int wIn_,
    int wOut_,
    string methodIn,
    double inputScale_,
    int adhocCompression_,
    bool expensiveSymmetry_)
      : Operator(parentOp_, target_), wIn(wIn_), wOut(wOut_), inputScale(inputScale_), adhocCompression(static_cast<Compression>(adhocCompression_)),
        expensiveSymmetry(expensiveSymmetry_)
  {
    // Check sanity of inputs
    if(inputScale <= 0) {
      throw(string("inputScale should be strictly positive"));
    }

    Method method = methodFromString(methodIn);
    ActivationFunction af = functionFromString(fIn);

    string* function;                 // Points to the function we need to approximate
    map<string, string> params = {};  // Map of parameters

    size_t in = 0, out = 0;           // Number the inputs and outputs to have a variable chain

    FunctionData fd = activationFunction.at(af);

    int lsbIn = -wIn + 1;               // The input is always signed, we need to account for it
    int lsbOut = -wOut + fd.signedOut;  // The output sign bit depends on the exact function
    bool signedIn = true;               // The input is always signed, i.e. in [-1,1)

    // The input might be scaled according to the input scaling
    if(fd.scaleFactor != 0.0) {
      int e;

      double fr = frexp(inputScale * fd.scaleFactor, &e);  // Recover the exponent and fraction

      // No output should touch exactly one,
      // so 2^n only needs n bits to be written
      if(fr == 0.5) e--;

      if(e > 0) lsbOut += e;  // We only reduce precision when the output can get bigger than 1
    }

    // adhocCompression is to use the difference to a known function to obtain smaller output values
    if((adhocCompression == Compression::Enabled) && (fd.deltaFunction == Delta::None)) {
      // the user asked to compress a function that is not compressible
      REPORT(LogLevel::MESSAGE, fd.longName << " has no simple delta to a fast function (ReLU or ReLU')." << endl);
      adhocCompression = Compression::Disabled;
    }

    if(adhocCompression == Compression::Auto) {
      // The compression is only enabled for functions that have a delta
      adhocCompression = static_cast<Compression>(fd.deltaFunction != Delta::None);
    }

    if(fd.incompatibleMethods.find(method) != fd.incompatibleMethods.end()) {
      REPORT(
        LogLevel::MESSAGE, "The requested method is not very compatible with the function selected, the computation might take a long time or fail.")
    }

    // Process the function definition based on what we know
    const string scaleString = "(" + to_string(inputScale) + "*@)";

    string base, deltaTo, delta;  // The standard formula, and the delta function associated to it

    base = fd.formula;

    switch(fd.deltaFunction) {
    case Delta::ReLU:
      deltaTo = activationFunction.at(ReLU).formula;
    case Delta::ReLU_P:
      deltaTo = activationFunction.at(ReLU_P).formula;
    case Delta::None:
      break;
    }

    // Scale the input accordingly
    base = replaceX(base, scaleString);
    deltaTo = replaceX(deltaTo, scaleString);

    // Rescale if necessary to avoid touching the limit
    if(fd.slightRescale) {
      base = "(1-1b" + to_string(lsbOut) + ")*(" + base + ")";
      deltaTo = "(1-1b" + to_string(lsbOut) + ")*(" + deltaTo + ")";
    }

    // Add the contribution of the derivative if necessary
    if(fd.derivative) {
      base = to_string(inputScale) + "*(" + base + ")";
      deltaTo = to_string(inputScale) + "*(" + deltaTo + ")";
    }

    if(af == ELU) {                               // ELU is a special case, the only complicated part is on the negative values
      delta = "-(" + replaceX(base, "-@") + ")";  // Thus, we need to flip ths function, to go to [0,1)
    } else {
      delta = "(" + deltaTo + ")-(" + base + ")";
    }

    // Underlying function definition, used to generate test cases
    f = new FixFunction(base, true, lsbIn, lsbOut);
    correctlyRounded = false;  // default is faithful

    // Cheat on the signedIn value, by exploiting symmetry to the max
    signedIn = false;  // We can exploit symetry on all the delta functions

    // When compression is enabled, compute the approximation of the delta function
    function = adhocCompression == Compression::Enabled ? &delta : &base;

    // means "please choose for me"
    if(method == Method::Auto) {
      // TODO a complex sequence of if then else once the experiments are done
      method = Method::PlainTable;
    }

    REPORT(LogLevel::MESSAGE,
      "Function after pre-processing: " << fd.longName << " evaluated on [-1,1)" << endl
                                        << " wIn=" << wIn << " translates to lsbIn=" << lsbIn << endl
                                        << " wOut=" << wOut << " translates to lsbOut=" << lsbOut);
    REPORT(LogLevel::MESSAGE, "Method is " << methodIn);

    if(adhocCompression == Compression::Enabled) {
      REPORT(LogLevel::MESSAGE,
        "To plot the function with its delta function, copy-paste the following lines in Sollya" << endl
                                                                                                 << "  f = " << sollyaFunction << ";" << endl
                                                                                                 << "  deltaf = " << sollyaDeltaFunction << ";"
                                                                                                 << endl
                                                                                                 << "  relu = " << sollyaReLU << ";" << endl
                                                                                                 << "  plot(deltaf, [-1;1]); " << endl
                                                                                                 << "  plot(f,relu,-deltaf, [-1;1]); ");
    } else {
      REPORT(LogLevel::MESSAGE,
        "To plot the function being implemented, copy-paste the following two lines in Sollya" << endl
                                                                                               << "  f = " << sollyaFunction << ";" << endl
                                                                                               << "  plot(f, [-1;1]); ");
    }

    correctlyRounded = false;  // default is faithful

    f = new FixFunction(sollyaFunction, true, lsbIn, lsbOut);

    ostringstream name;
    name << fd.name << "_" << wIn << "_" << wOut << "_" << methodIn;
    setNameWithFreqAndUID(name.str());

    setCopyrightString("Florent de Dinechin (2023)");

    addInput(input(in), wIn);
    addOutput("Y", wOut);

    params["signedIn"] = to_string(signedIn);
    params["lsbOut"] = to_string(lsbOut);

    if(af == ReLU) {
      // Special case for ReLU
      // TODO: Take into account rounding if necessary
      vhdl << tab << "Y <= " << relu(wIn, wOut);
      return;
    }

    if(af == ReLU_P) {
      // TODO: Use a multiplication by a constant ?
      vhdl << tab << "Y <= " << relu(wIn, wOut, true);
      return;
    }

    if(adhocCompression == Compression::Enabled) {
      if(wIn != wOut) {
        throw(string("Too lazy so far to support wIn<>wOut in case of ad-hoc compression "));
      };

      // Declare the ReLU signal, when the function is a derivative, we use ReLU_P instead
      // TODO: verify that it really works with different inputScales
      vhdl << tab << declare("ReLU", wOut) << " <= " << relu(wIn, wOut, fd.deltaFunction == Delta::ReLU_P);
    }

    switch(method) {
    case Method::PlainTable: {
      addComment("This function is correctly rounded");
      correctlyRounded = true;
      break;
    }
    case Method::MultiPartite: {
      break;
    }
    case Method::Horner: {
      // When compressing, we only deal with the positive interval so we rescale to the whole interval
      if(adhocCompression == Compression::Enabled) {
        *function = replaceX(*function, "((@+1)/2)");
        lsbIn++;                   // The scaling means that we shift the fixed value
        params["signedIn"] = "1";  // Force the input to be signed, it will be scaled accordingly
      }
      break;
    }
    case Method::PiecewiseHorner2: {
      params["d"] = "2";
      signedIn = false;
      break;
    }
    case Method::PiecewiseHorner3: {
      params["d"] = "3";
      signedIn = false;
      break;
    }
    default:
      throw(string("Method: ") + methodIn + " currently unsupported");
      break;
    };

    params["f"] = *function;
    params["lsbIn"] = to_string(lsbIn);

    if(!signedIn) {
      // Compute the absolute value of X
      in++;
      vhdl << tab << declare(input(in), wIn) << " <= (not(" << input(in - 1) << ") + (" << zg(wIn - 1) << " & '1')) when " << input(in - 1)
           << of(wIn - 1) << " = '1' else " << input(in - 1) << ";" << endl;
      in++;
      vhdl << tab << declare(input(in), wIn - 1) << " <= " << input(in - 1) << range(wIn - 2, 0) << ";" << endl;

      if(method == Method::Horner) {
        // We need to scale the input
        in++;
        vhdl << tab << declare(input(in), wIn - 1) << " <= not(" << input(in - 1) << of(wIn - 2) << ") & " << input(in - 1) << range(wIn - 3, 0)
             << ";" << endl;
      }
    }

    string paramString;

    for(const auto& [key, value]: params) {
      paramString += key + "=" + value + " ";
    }

    OperatorPtr op = newInstance(methodOperator(method),
      fd.name + (adhocCompression == Compression::Enabled ? "_delta_SNAFU" : "_SNAFU"),
      paramString,
      "X => " + input(in),
      adhocCompression == Compression::Enabled ? "Y => D" : "Y => F");

    if(adhocCompression == Compression::Enabled) {
      // Reconstruct the function
      int deltaOutSize = getSignalByName("D")->width();

      if(af == ELU) {
        // Special case, when X is positive, then it is identical to the ReLU
        vhdl << tab << declare("F0", wOut) << " <= ReLU when X" << of(wIn - 1) << " = '0' else ReLU - (" << zg(wOut - deltaOutSize) << " & D);"
             << endl;
      } else {
        // if all went well deltaOut should have fewer bits so we need to pad with zeroes
        vhdl << tab << declare("F0", wOut) << " <= ReLU - (" << zg(wOut - deltaOutSize) << " & D);" << endl;
      }

      // We are running in symmetry mode
      if(expensiveSymmetry) {
        mpz_class rc, ru;
        // TODO: fix for ELU
        f->eval(mpz_class(1) << (wIn - 1), rc, ru, true);
        vhdl << tab << declare("F", wOut) << " <= (" << getSignalByName("F0")->valueToVHDL(rc) << ") when (ieee.std_logic_misc.or_reduce(X"
             << range(wIn - 2, 0) << ") = '0' and X" << of(wIn - 1) << " = '1') else F0;" << endl;
      } else {
        vhdl << tab << declare("F", wOut) << " <= F0;" << endl;
      }

    } else {
      // sanity check
      if(int w = op->getSignalByName("X")->width() != wIn) {
        REPORT(LogLevel::MESSAGE,
          "Something went wrong, operator input size is  " << w << " instead of the requested wIn=" << wIn << endl
                                                           << "Attempting to proceed nevertheless.");
      }

      if(int w = op->getSignalByName("Y")->width() != wOut) {
        REPORT(LogLevel::MESSAGE,
          "Something went wrong, operator output size is  " << w << " instead of the requested wOut=" << wOut << endl
                                                            << "Attempting to proceed nevertheless.");
      }
    }

    vhdl << tab << "Y <= F;" << endl;
  }


  // Boilerplate and arguments
  void SNAFU::emulate(TestCase* tc)
  {
    emulate_fixfunction(*f, tc, correctlyRounded);
  }

  OperatorPtr SNAFU::parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args, UserInterface& ui)
  {
    int wIn, wOut;
    double inputScale;
    string fIn, methodIn;
    int adhocCompression;
    bool expensiveSymmetry;
    ui.parseString(args, "f", &fIn);
    ui.parseInt(args, "wIn", &wIn);
    ui.parseInt(args, "wOut", &wOut);
    ui.parseString(args, "method", &methodIn);
    ui.parseFloat(args, "inputScale", &inputScale);
    ui.parseInt(args, "adhocCompression", &adhocCompression);
    ui.parseBoolean(args, "expensiveSymmetry", &expensiveSymmetry);
    return new SNAFU(parentOp, target, fIn, wIn, wOut, methodIn, inputScale, adhocCompression, expensiveSymmetry);
  }


  template <>
  const OperatorDescription<SNAFU> op_descriptor<SNAFU>{
    "SNAFU",   // name
    "Simple Neural Activation Function Unit, without reinventing the wheel",
    "Hidden",  // some day: FunctionApproximation ?
    "Also see generic options",
    "f(string): function of interest, among \"Tanh\", \"Sigmoid\", \"ReLU\", \"GELU\", \"ELU\", \"SiLU\" (case-insensitive);"
    "wIn(int): number of bits of the input ;"
    "wOut(int): number of bits of the output; "
    "expensiveSymmetry(bool)=false: whether to add a special case for the input -1 when symmetry is used as 1 is not a representable fixed point;"
    "inputScale(real)=8.0: the input scaling factor: the 2^wIn input values are mapped on the interval[-inputScale, inputScale) ; "
    "method(string)=auto: approximation method, among \"PlainTable\",\"MultiPartite\", \"Horner\", \"PiecewiseHorner2\", \"PiecewiseHorner3\", "
    "\"auto\" ;"
    "adhocCompression(int)=-1: 1: subtract the base function ReLU to implement only the non-linear part. 0: do nothing. -1: automatic;",
    "",
  };


}  // namespace flopoco
