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

static inline void _replace(string& s, const string x, const string r, const size_t c = 1)
{
  size_t pos;
  while((pos = s.find(x)) != string::npos) {
    s.replace(pos, c, r);
  }
}

// Replace X with r, assuming future Xs are symbolized by @ in r
static inline void replaceX(string& s, const string r)
{
  _replace(s, "X", r);
  _replace(s, "@", "X");
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
    int useDeltaReLU_,
    bool enableSymmetry,
    bool plotFunction)
      : Operator(parentOp_, target_), useDeltaReLU(static_cast<DeltaReLUCompression>(useDeltaReLU_))
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

    bool signedIn = fd.signedIn;        // The input is almost always signed, i.e. in [-1,1) (only InvExp is exception)
    int lsbIn = -wIn + signedIn;        // The input is always signed, we need to account for it
    int lsbOut = -wOut + fd.signedOut;  // The output sign bit depends on the exact function

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

    // useDeltaReLU is to use the difference to a known function to obtain smaller output values
    if((useDeltaReLU == DeltaReLUCompression::Enabled) && (fd.deltaFunction == Delta::None)) {
      // the user asked to compress a function that is not compressible
      REPORT(LogLevel::MESSAGE, fd.longName << " has no simple delta to a fast function (ReLU or ReLU'), attempting to compress anyway." << endl);
      // useDeltaReLU = DeltaReLUCompression::Disabled;
    }

    if(useDeltaReLU == DeltaReLUCompression::Auto) {
      // The DeltaReLUCompression is only enabled for functions that have a delta
      useDeltaReLU = static_cast<DeltaReLUCompression>(fd.deltaFunction != Delta::None);
      if(af == GeLU && wOut <= 6) {
        useDeltaReLU = DeltaReLUCompression::Disabled;
      }
    }

    if(fd.incompatibleMethods.find(method) != fd.incompatibleMethods.end()) {
      REPORT(
        LogLevel::MESSAGE, "The requested method is not very compatible with the function selected, the computation might take a long time or fail.")
    }

    // Tackle symmetry, the symmetry is considered after reducing the function all the way, i.e. after delta and offset manipulations
    const bool cond = fd.offset != 0.0 || fd.deltaFunction == Delta::None;
    bool useSymmetry = enableSymmetry && (useDeltaReLU == DeltaReLUCompression::Enabled ? fd.deltaParity != Parity::None : fd.parity != Parity::None);

    // Process the function definition based on what we know
    const string scaleString = "(" + to_string(inputScale) + "*(1+1b" + to_string(lsbIn) + ")*@)";

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

    // Disable the threshold part when we exploit symmetry
    if(useSymmetry) {
      _replace(base, "exp(-1b256*X)", "0", 13);
      _replace(deltaTo, "exp(-1b256*X)", "0", 13);
    }

    // Scale the input accordingly
    replaceX(base, scaleString);
    replaceX(deltaTo, scaleString);

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

    delta = "(" + deltaTo + ")-(" + base + ")";

    // REPORT(LogLevel::MESSAGE, "\t     lsbIn=" << lsbIn << "  lsbdOut=" << lsbOut);

    // Underlying function definition, used to generate test cases
    f = new FixFunction(base, signedIn, lsbIn, lsbOut);
    correctlyRounded = false;  // default is faithful

    string base_elu = "expm1(I*(X-1))";
    if(af == ELU) {
      // In the case of ELU, we only need  to compute something for the negative X
      // This is a smört trick to do it
      _replace(base_elu, "I", to_string(inputScale));
      signedIn = false;
      function = &base_elu;
    } else {
      // Only compute the delta function if one is defined
      if(useDeltaReLU == DeltaReLUCompression::Enabled && fd.deltaFunction != Delta::None) {
        function = &delta;  // The function to really approximate is the delta one, the rest is only tricks
      } else {
        // Respect the whishes of the user
        function = &base;
      }
    }

    // means "please choose for me"
    if(method == Method::Auto) {
      // TODO: a complex sequence of if then else once the experiments are done
      method = Method::PlainTable;
    }
    // Print a summary
    REPORT(LogLevel::MESSAGE, "Function after pre-processing: " << fd.longName << " evaluated on " << (f->signedIn ? "[-1,1)" : "[0,1)"));
    REPORT(LogLevel::MESSAGE, "\twIn=" << wIn << " translates to lsbIn=" << lsbIn);
    REPORT(LogLevel::MESSAGE, "\twOut=" << wOut << " translates to lsbOut=" << lsbOut);
    REPORT(LogLevel::MESSAGE, "\t  f->wOut=" << f->wOut << "  f->signedOut=" << f->signedOut);

    REPORT(LogLevel::MESSAGE, "Method is " << methodIn);

    if(useDeltaReLU == DeltaReLUCompression::Enabled) {
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
                                                                                               << "  plot(f, " << (signedIn ? "[-1;1]" : "[0;1]")
                                                                                               << "); ");
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

    if(useDeltaReLU == DeltaReLUCompression::Enabled && fd.deltaFunction != Delta::None) {
      if(wIn != wOut) {
        throw(string("Too lazy so far to support wIn<>wOut in case of ad-hoc DeltaReLUCompression "));
      };

      // Declare the ReLU signal, when the function is a derivative, we use ReLU_P instead
      // TODO: verify that it really works with different inputScales
      // FIXME: It is sure to not work when inputScale is not a power of two
      vhdl << tab << declare("ReLU", wOut) << " <= " << relu_fd(wIn, wOut, fd);
    }

    if(useSymmetry) {
      REPORT(LogLevel::MESSAGE, "Symmetry enabled.")
    } else {
      REPORT(LogLevel::MESSAGE, "Symmetry disabled.")
    }

    switch(method) {
    case Method::PlainTable: {
      REPORT(LogLevel::MESSAGE, "Method is FixFunctionByPlainTable");
      // addComment("This function is correctly rounded");
      correctlyRounded = true;
      break;
    }
    case Method::MultiPartite: {
      REPORT(LogLevel::MESSAGE, "Method is FixFunctionByMultiPartite");
      // params["d"] = "1";
      break;
    }
    case Method::Horner: {
      REPORT(LogLevel::MESSAGE, "Method is Horner");
      // FIXME: Make Horner work
      break;
    }
    case Method::PiecewiseHorner1: {
      REPORT(LogLevel::MESSAGE, "Method is FixFunctionByPiecewisePoly, Horner evaluation, degree 1");
      params["d"] = "1";
      forceRescale = true;
      break;
    }
    case Method::PiecewiseHorner2: {
      REPORT(LogLevel::MESSAGE, "Method is FixFunctionByPiecewisePoly, Horner evaluation, degree 2");
      params["d"] = "2";
      forceRescale = true;
      break;
    }
    case Method::PiecewiseHorner3: {
      REPORT(LogLevel::MESSAGE, "Method is FixFunctionByPiecewisePoly, Horner evaluation, degree 3");
      params["d"] = "3";
      forceRescale = true;
      break;
    }
    default:
      throw("Method: " + methodIn + " currently unsupported");
      break;
    };

    // If we intend on using symmetry, only send the absolute value (modulo -1) in the operator
    if(useSymmetry) {
      signedIn = false;  // We known how to exploit the symmetries

      // Compute the absolute value of X
      size_t x = in;
      size_t a = ++in;

      // As the interval is slightly shifted, to get -X we only need to negate the bits
      vhdl << tab << declare(input(a), wIn) << " <= not(" << input(x) << ") when " << input(x) << of(wIn - 1) << " = '1' else " << input(x) << ";"
           << endl;

      vhdl << tab << declare(input(++in), wIn - 1) << " <= " << input(a) << range(wIn - 2, 0) << ";" << endl;
    } else if(forceRescale && af != ELU) {  // This is incompatible with the exploitation of symmetry
      // The original input is in [-1,1) but for technical reasons, we need to set it in [0,1)

      replaceX(*function, "(2*@-1)");  // Mathematical rescaling of the function
      lsbIn--;                         // lsbIn needs to be updated as we shift everything 1 bit
      signedIn = false;

      const size_t x = in;
      const size_t s = ++in;
      auto X = getSignalByName(input(x));
      int w = X->width();

      vhdl << tab << declare(input(s), w) << " <= ('0' & " << input(x) << range(w - 2, 0) << ") when " << input(x) << of(w - 1)
           << " = '1' else ('1' & " << input(x) << range(w - 2, 0) << ");" << endl;
    }

    if(af == ELU) {
      // Map [-1, 0) to [0, 1) by dropping the leading bit
      size_t x = in;
      size_t a = ++in;

      vhdl << tab << declare(input(a), wIn - 1) << " <= " << input(x) << range(wIn - 2, 0) << ";" << endl;
    }

    params["f"] = *function;
    params["signedIn"] = to_string(signedIn);
    params["lsbIn"] = to_string(lsbIn);
    string paramString;

    for(const auto& [key, value]: params) {
      paramString += key + "=" + value + " ";
    }

    REPORT(LogLevel::MESSAGE, paramString);

    OperatorPtr op = newInstance(methodOperator(method),
      fd.name + (useDeltaReLU == DeltaReLUCompression::Enabled ? "_delta_SNAFU" : "_SNAFU"),
      paramString,
      "X => " + input(in),
      "Y => " + output(out));

    // Extends the signal to the full width if necessary
    auto C = getSignalByName(output(out));

    if(C->width() < wOut) {
      size_t y = ++out;

      vhdl << tab << declare("E", wOut - C->width());
      auto E = getSignalByName("E");

      // When do we need to do a sign extension :
      if(af == ELU) {
        vhdl << " <= " << E->valueToVHDL(-1) << ";" << endl;
      } else if((useDeltaReLU == DeltaReLUCompression::Enabled && fd.signedDelta) ||
        (useDeltaReLU == DeltaReLUCompression::Disabled && fd.signedOut && !useSymmetry)) {
        vhdl << " <= " << E->valueToVHDL(0) << " when " << C->getName() << of(C->width() - 1) << " = '0' else " << E->valueToVHDL(-1) << ";" << endl;
      } else {
        vhdl << " <= " << E->valueToVHDL(0) << ";" << endl;
      }

      vhdl << tab << declare(output(y), wOut) << " <= E & " << C->getName() << ";" << endl;
    }

    if(useSymmetry && (fd.parity == Parity::Odd || fd.deltaParity == Parity::Odd)) {
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

    if(useDeltaReLU == DeltaReLUCompression::Enabled && fd.deltaFunction != Delta::None) {
      // Reconstruct the function
      size_t d = out;
      size_t f = ++out;

      auto D = getSignalByName(output(d));

      // // if all went well deltaOut should have fewer bits so we need to pad with zeroes
      vhdl << tab << declare(output(f), wOut) << " <= ReLU - (" + output(d) + ");" << endl;
    }

    // Reconstruct the ELU value with a simple mux
    if(af == ELU) {
      size_t d = out;
      size_t e = ++out;

      vhdl << tab << declare(output(e), wOut) << " <= " << output(d) << " when X" << of(wIn - 1) << " = '1' else X;" << endl;
    }

    vhdl << tab << "Y <= " << output(out);

    vhdl << ";" << endl;


    // Add the plot with gnuplot
    if (plotFunction) {
    
      // Start with the gnuplot file
      ostringstream filename1;
      filename1 << "plot_"<<vhdlize(f->description) << ".gp";
      fstream d1;
      d1.open(filename1.str().c_str(), ios::out);  // no precautions here, this is not prod code
      d1 << "set title \"f(x)=" << f->description << "\"\nset xlabel \"x\"\nset ylabel \"f(x)\"\nset grid\nplot \"plot_" << vhdlize(f->description)  << ".dat\" using 1:2 title \"\" lt 1 pt 7 ps 1";
      if (!correctlyRounded) {
        d1 << ", \"plot_" << vhdlize(f->description)  << ".dat\" using 1:3 title \"\" lt 2 pt 7 ps 1";
      }
      d1.close();
  
      // Now the data file
      ostringstream filename;
      filename << "plot_"<<vhdlize(f->description) << ".dat";
      fstream d;
      d.open(filename.str().c_str(), ios::out);  // no precautions here, this is not prod code
      
      mpz_class sign_limit_in = (mpz_class(1)<<(wIn-1));
      mpz_class sign_limit_out = (mpz_class(1)<<(wOut-1));
      mpz_class max_input = (mpz_class(1)<<(wIn));
      mpz_class max_output = (mpz_class(1)<<(wOut));

      for (mpz_class i = 0; i < max_input; i++) {
        mpz_class x = i;
	      mpz_class rNorD,ru;
	      f->eval(x,rNorD,ru,correctlyRounded);
        // Signed input 
        if (x >= sign_limit_in) {
          x = x - max_input;
        }
        // Signed output
        if (fd.signedOut) {
          if (rNorD >= sign_limit_out) {
            rNorD = rNorD - max_output;
          }
          if (ru >= sign_limit_out) {
            ru = ru - max_output;
          }
        }

        d << x << " " << rNorD;
	      if(!correctlyRounded)
          d << " " << ru;
        d << endl;
      }
      
      d.close();

      REPORT(LogLevel::MESSAGE, "To plot function, either execute 'gnuplot -c \"plot_"<<vhdlize(f->description) << ".gp\"' in terminal or 'load \"plot_"<<vhdlize(f->description) << ".gp\"' in gnuplot.");
    }
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
    int useDeltaReLU;
    bool enableSymmetry;
    bool plotFunction;
    ui.parseString(args, "f", &fIn);
    ui.parseInt(args, "wIn", &wIn);
    ui.parseInt(args, "wOut", &wOut);
    ui.parseString(args, "method", &methodIn);
    ui.parseFloat(args, "inputScale", &inputScale);
    ui.parseInt(args, "useDeltaReLU", &useDeltaReLU);
    ui.parseBoolean(args, "enableSymmetry", &enableSymmetry);
    ui.parseBoolean(args, "plotFunction", &plotFunction);
    return new SNAFU(parentOp, target, fIn, wIn, wOut, methodIn, inputScale, useDeltaReLU, enableSymmetry, plotFunction);
  }

  TestList SNAFU::unitTest(int testLevel)
  {
    // the static list of mandatory tests
    TestList testStateList;
    vector<pair<string, string>> paramList;
    std::vector<std::array<int, 5>> paramValues, moreParamValues;  //  order is win, wout, method, useDeltaReLU
                                                                   // TODO method currently ignored
    paramValues = {
      // testing the default value on the most standard cases
      {6, 6, 0, 0},
      {6, 6, 0, 1},
      {8, 8, 0, 0},
      {8, 8, 0, 1},
    };

    if(testLevel == TestLevel::QUICK) {
      // just test paramValues
    }
    if(testLevel >= TestLevel::SUBSTANTIAL) {
      // same tests but add the other SRT values
    }
    if(testLevel >= TestLevel::EXHAUSTIVE) { }
    // Now actually build the paramValues structure
    for(auto f: activationFunction) {
      for(auto params: paramValues) {
        paramList.push_back(make_pair("f", f.second.name));
        paramList.push_back(make_pair("wIn", to_string(params[0])));
        paramList.push_back(make_pair("wOut", to_string(params[1])));
        string method = "auto";
        paramList.push_back(make_pair("method", method));
        paramList.push_back(make_pair("useDeltaReLU", to_string(params[3])));
        testStateList.push_back(paramList);
        // cerr << " " << params[0]  << " " << params[1]  << " " << params[2] << endl;
        paramList.clear();
      }
    }
    return testStateList;
  }


  template <>
  const OperatorDescription<SNAFU> op_descriptor<SNAFU>{
    "SNAFU",               // name
    "Simple Neural Activation Function Unit, without reinventing the wheel",
    "BasicFloatingPoint",  // some day: FunctionApproximation ?
    "Also see generic options",
    "f(string): function of interest, among \"Tanh\", \"Sigmoid\", \"ReLU\", \"GELU\", \"ELU\", \"SiLU\", \"InvExp\" (case-insensitive);"
    "wIn(int): number of bits of the input ;"
    "wOut(int): number of bits of the output; "
    "plotFunction(bool)=false: generate files to plot the function. Not recommended for wIn > 12; "
    "enableSymmetry(bool)=false: whether to use the intrinsic symmetry of the function to compress a little the result;"
    "inputScale(real)=8.0: the input scaling factor: the 2^wIn input values are mapped on the interval[-inputScale, inputScale) ; "
    "method(string)=auto: approximation method, among \"PlainTable\",\"MultiPartite\", \"Horner\", \"PiecewiseHorner1\", \"PiecewiseHorner2\", "
    "\"PiecewiseHorner3\", "
    "\"auto\" ;"
    "useDeltaReLU(int)=-1: 1: subtract the base function ReLU to implement only the non-linear part. 0: do nothing. -1: automatic;",
    "",
  };


}  // namespace flopoco
