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
#include "flopoco/utils.hpp"

#include <cmath>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>

using namespace std;


#define LARGE_PREC 1000  // 1000 bits should be enough for everybody

// Definition of a ReLU specific to the function data
static inline const string relu_fd(int wIn, int wOut, FunctionData& fd)
{
  // Base case, we return 0 for all negative values
  string s = zg(wOut) + " when X" + of(wIn - 1) + " = '1' else ";

  // If the deltaTo function is the simple ReLU, return X for positive values
  if(fd.deltaFunction == Delta::ReLU) {
    return s + "X;\n";
  }

  // If we need to do a slight rescale of the function, the ReLU is all ones
  if(fd.slightRescale) {
    return s + zg(1) + " & " + og(wOut - 1) + ";\n";
  }

  // If we don't rescale, it means that we don't hit a power of two

  return s + "(\"01\" & " + zg(wOut - 2) + ");\n";
};

// Mux definition for the ReLU
static inline const string relu(int wIn, int wOut, bool derivative = false, bool rescale = false)
{
  std::ostringstream s;
  s << zg(wOut) << " when X" << of(wIn - 1) << " = '1' else '0' & ";

  if(derivative) {
    if(rescale) {
      s << zg(1) << " & " << og(wIn - 2);
    } else {
      s << og(wIn - 1);
    }
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
    int wIn,
    int wOut,
    string methodIn,
    double inputScale,
    int adhocCompression_,
    bool expensiveSymmetry)
      : Operator(parentOp_, target_), adhocCompression(static_cast<Compression>(adhocCompression_))
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

    bool forceRescale = false;          // When the internal operator only works on [0,1), e.g. Horner & PieceWiseHorner

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
      break;
    case Delta::ReLU_P:
      deltaTo = activationFunction.at(ReLU_P).formula;
      break;
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

    if(adhocCompression == Compression::Enabled) {
      function = &delta;       // The function to really approximate is the delta one, the rest is only tricks
    } else {
      // Respect the whishes of the user
      function = &base;
    }

    // means "please choose for me"
    if(method == Method::Auto) {
      // TODO: a complex sequence of if then else once the experiments are done
      method = Method::PlainTable;
    }

    // Print a summary
    REPORT(LogLevel::MESSAGE, "Function after pre-processing: " << fd.longName << " evaluated on [-1,1)");
    REPORT(LogLevel::MESSAGE, "\twIn=" << wIn << " translates to lsbIn=" << lsbIn);
    REPORT(LogLevel::MESSAGE, "\twOut=" << wOut << " translates to lsbOut=" << lsbOut);

    REPORT(LogLevel::MESSAGE, "Method is " << methodIn);

    if(adhocCompression == Compression::Enabled) {
      REPORT(LogLevel::MESSAGE,
        "To plot the function with its delta function,"
          << " copy-paste the following lines in Sollya:" << endl
          << "\tf = " << base << ";" << endl
          << "\tdeltaf = " << delta << ";" << endl
          << "\trelu = " << deltaTo << ";" << endl
          << "\tplot(deltaf, [-1;1]); " << endl
          << "\tplot(f,relu,-deltaf, [-1;1]);");
    } else {
      REPORT(LogLevel::MESSAGE,
        "To plot the function being implemented, copy-paste the following two lines in Sollya" << endl
                                                                                               << "  f = " << base << ";" << endl
                                                                                               << "  plot(f, [-1;1]); ");
    }

    ostringstream name;
    name << fd.name << "_" << wIn << "_" << wOut << "_" << methodIn;

    setNameWithFreqAndUID(name.str());
    setCopyrightString("Tom Hubrecht, Florent de Dinechin (2024)");

    addInput("X", wIn);
    addOutput("Y", wOut);

    vhdl << tab << declare(input(0), wIn) << " <= X;" << endl;

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
      // FIXME: It is sure to not work when inputScale is not a power of two
      vhdl << tab << declare("ReLU", wOut) << " <= " << relu_fd(wIn, wOut, fd);
    }

    // Tackle symmetry, the symmetry is considered after reducing the function all the way, i.e. after delta and offset manipulations
    const bool cond = fd.offset != 0.0 || fd.deltaFunction == Delta::None;
    const bool enableSymmetry = fd.parity != Parity::None && (!cond && adhocCompression == Compression::Enabled);

    if(enableSymmetry) {
      REPORT(LogLevel::MESSAGE, "Symmetry enabled.")
    } else {
      REPORT(LogLevel::MESSAGE, "Symmetry disabled.")
    }

    switch(method) {
    case Method::PlainTable: {
      // addComment("This function is correctly rounded");
      // When using compression, the best we can guarantee is faithful rounding
      correctlyRounded = adhocCompression == Compression::Disabled;
      break;
    }
    case Method::MultiPartite: {
      break;
    }
    case Method::Horner: {
      break;
    }
    case Method::PiecewiseHorner2: {
      params["d"] = "2";
      forceRescale = true;
      break;
    }
    case Method::PiecewiseHorner3: {
      params["d"] = "3";
      forceRescale = true;
      break;
    }
    default:
      throw("Method: " + methodIn + " currently unsupported");
      break;
    };

    // If we inted on using symmetry, only send the absolute value (modulo -1) in the operator
    if(enableSymmetry) {
      signedIn = false;  // We known how to exploit the symmetries

      // Compute the absolute value of X
      size_t x = in;
      size_t a = ++in;

      vhdl << tab << declare(input(a), wIn) << " <= (not(" << input(x) << ") + " << getSignalByName(input(x))->valueToVHDL(1) << ") when " << input(x)
           << of(wIn - 1) << " = '1' else " << input(x) << ";" << endl;

      vhdl << tab << declare(input(++in), wIn - 1) << " <= " << input(a) << range(wIn - 2, 0) << ";" << endl;
    } else if(forceRescale) {  // This is incompatible with the exploitation of symmetry
      // The original input is in [-1,1) but for technical reasons, we need to set it in [0,1)

      *function = replaceX(*function, "((@+1)/2)");  // Mathematical rescaling of the function
      lsbIn--;                                       // lsbIn needs to be updated as we shift everything 1 bit
      signedIn = false;

      const size_t x = in;
      const size_t s = ++in;
      auto X = getSignalByName(input(x));
      int w = X->width();

      vhdl << tab << declare(input(s), w) << " <= not(" << input(x) << of(w - 1) << ") & " << input(x) << range(w - 2, 0) << " when " << input(x)
           << of(w - 1) << " = '0' else (not(" << input(x) << range(w - 2, 0) << ") + " << X->valueToVHDL(1) << ");" << endl;
    }

    // if(!signedIn) {
    //   // TODO use forceRescale
    //   if(method == Method::Horner) {
    //     // We need to scale the input
    //     in++;
    //     vhdl << tab << declare(input(in), wIn - 1) << " <= not(" << input(in - 1) << of(wIn - 2) << ") & " << input(in - 1) << range(wIn - 3, 0)
    //          << ";" << endl;
    //   }
    // }

    params["f"] = *function;
    params["signedIn"] = to_string(signedIn);
    params["lsbIn"] = to_string(lsbIn);
    params["verbose"] = "3";
    string paramString;

    for(const auto& [key, value]: params) {
      paramString += key + "=" + value + " ";
    }

    REPORT(LogLevel::MESSAGE, paramString);

    OperatorPtr op = newInstance(methodOperator(method),
      fd.name + (adhocCompression == Compression::Enabled ? "_delta_SNAFU" : "_SNAFU"),
      paramString,
      "X => " + input(in),
      "Y => " + output(out));

    // Extends the signal to the full width
    if(adhocCompression == Compression::Enabled) {
      size_t c = out;
      size_t y = ++out;

      auto C = getSignalByName(output(c));

      vhdl << tab << declare("E", wOut - C->width());
      auto E = getSignalByName("E");

      if(fd.signedDelta) {
        vhdl << " <= " << E->valueToVHDL(0) << " when " << output(c) << of(C->width() - 1) << " = '0' else " << E->valueToVHDL(-1) << ";" << endl;
      } else {
        vhdl << " <= " << E->valueToVHDL(0) << ";" << endl;
      }

      vhdl << tab << declare(output(y), wOut) << " <= E & " << output(c) << ";" << endl;
    }

    if(enableSymmetry && fd.parity == Parity::Odd) {
      // Reconstruct the function based on the required symmetry, this is only required for odd functions
      const size_t a = out;
      const size_t f = ++out;
      auto A = getSignalByName(output(a));

      vhdl << tab << declare(output(f), A->width()) << " <= " << output(a) << " when X" << of(wIn - 1) << " = '0' else (not(" << output(a) << ") + "
           << A->valueToVHDL(1) << ");" << endl;
    }

    if(fd.offset != 0.0) {
      // TODO: Take into account potential offsets
    }

    if(adhocCompression == Compression::Enabled) {
      // Reconstruct the function
      size_t d = out;
      size_t f = ++out;

      auto D = getSignalByName(output(d));

      if(af == ELU) {
        // Special case, when X is positive, then it is identical to the ReLU
        vhdl << tab << declare(output(f), wOut) << " <= ReLU when X" << of(wIn - 1) << " = '0' else ReLU - (" + output(d) + ");" << endl;
      } else {
        // if all went well deltaOut should have fewer bits so we need to pad with zeroes
        vhdl << tab << declare(output(f), wOut) << " <= ReLU - (" + output(d) + ");" << endl;
      }
    }

    // We are running in symmetry mode
    if(expensiveSymmetry && enableSymmetry) {
      mpz_class rc, ru;
      // TODO: fix for ELU ???
      f->eval(mpz_class(1) << (wIn - 1), rc, ru, true);  // Compute f(-1), which is '10...0' in 2's complement

      size_t f = out;
      size_t y = ++out;

      addComment("Expensive reconstruction for -1");
      vhdl << tab << declare(output(y), wOut) << " <= " << getSignalByName(output(f))->valueToVHDL(rc)
           << " when X = " << getSignalByName("X")->valueToVHDL(mpz_class(1) << wIn - 1) << " else " << output(f) << ";" << endl;
    }

    vhdl << tab << "Y <= " << output(out);

    vhdl << ";" << endl;
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
