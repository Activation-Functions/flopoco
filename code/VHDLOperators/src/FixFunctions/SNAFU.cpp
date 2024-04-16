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

using namespace std;


#define LARGE_PREC 1000  // 1000 bits should be enough for everybody


// Mux definition for the ReLU
static inline const string relu(int wIn, int wOut)
{
  std::ostringstream s;
  s << zg(wOut) << " when X" << of(wIn - 1) << "='1'   else '0' & X" << range(wIn - 2, 0) << ";" << endl;

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
  return n ? "X" + to_string(n) : "X";
}

namespace flopoco
{
  enum ActivationFunction {
    Sigmoid,
    Sigmoid_P,
    TanH,
    TanH_P,
    ReLU,
    ReLU_P,
    SiLU,
    SiLU_P,
    GeLU,
    GeLU_P,
    ELU,
    ELU_P,
  };

  struct FunctionData {
    string name;
    string longName;
    string formula;
    ActivationFunction fun;
    bool signedOut;
    bool reluVariant = false;    // By default, not a ReLU variant
    bool slightRescale = false;  // for output range efficiency of functions that touch 1
    double scaleFactor = 0.0;    // The importancee of tne inputScale
    bool derivative = false;     // Whether the function is a derivative, and we need to multiply the output by the inputScale
  };

  static const map<string, FunctionData> activationFunction = {
    // two annoyances below:
    // 1/ we are not allowed spaces because it would mess the argument parser. Silly
    // 2/ no if then else in sollya, so we emulate with the quasi-threshold function 1/(1+exp(-1b256*X))
    {"sigmoid",
      FunctionData{
        .name = "Sigmoid",
        .longName = "Sigmoid",
        .formula = "1/(1+exp(-X))",  //textbook
        .fun = Sigmoid,              // enum
        .signedOut = false,          // output unsigned
        .slightRescale = true,       // output touches 1 and needs to be slightly rescaled
      }},
    {"tanh",
      FunctionData{
        .name = "TanH",
        .longName = "Hyperbolic Tangent",
        .formula = "tanh(X)",
        .fun = TanH,            // enum
        .signedOut = true,      // output signed
        .slightRescale = true,  // output touches 1 and needs to be slightly rescaled
      }},
    {"relu",
      FunctionData{
        .name = "ReLU",
        .longName = "Rectified Linear Unit",
        .formula = "X/(1+exp(-1b256*X))",  // Here we use a quasi-threshold function
        .fun = ReLU,                       // enum
        .signedOut = false,                // output unsigned
        .scaleFactor = 1.0,                //
      }},
    {"elu",
      FunctionData{
        .name = "ELU",
        .longName = "Exponential Linear Unit",
        .formula = "X/(1+exp(-1b256*X))+expm1(X)*(1-1/(1+exp(-1b256*X)))",  // Here we use a quasi-threshold function
        .fun = ELU,                                                         // enum
        .signedOut = true,                                                  // output signed
        .reluVariant = true,                                                // ReLU variant
        .scaleFactor = 1.0,                                                 //
      }},
    {"silu",
      FunctionData{
        .name = "SiLU",
        .longName = "Sigmoid Linear Unit",
        .formula = "X/(1+exp(-X))",  // textbook
        .fun = SiLU,                 // enum
        .signedOut = true,           // output signed
        .reluVariant = true,         // ReLU variant
        .scaleFactor = 1.0,          //
      }},
    {"gelu",
      FunctionData{
        .name = "GeLU",
        .longName = "Gaussian Error Linear Unit",
        .formula = "(X/2)*(1+erf(X/sqrt(2)))",  // textbook
        .fun = GeLU,                            // enum
        .signedOut = true,                      // output signed
        .reluVariant = true,                    // ReLU variant
        .scaleFactor = 1.0,                     //
      }},

    // Derivatives
    {"sigmoid_p",
      FunctionData{
        .name = "Sigmoid_P",
        .longName = "Derivative Sigmoid",
        .formula = "exp(-X)/(1+exp(-X))^2",  //textbook
        .fun = Sigmoid_P,                    // enum
        .signedOut = false,                  // output unsigned
        .slightRescale = true,
        .scaleFactor = 0.25,
        .derivative = true,
      }},
    {"tanh_p",
      FunctionData{
        .name = "TanH_P",
        .longName = "Derivative Hyperbolic Tangent",
        .formula = "(1-tanh(X)^2)",  //textbook
        .fun = TanH_P,               // enum
        .signedOut = false,          // output unsigned
        .slightRescale = true,       // output touches 1 and needs to be slightly rescaled
        .scaleFactor = 1.0,          // Function is a derivative, so we need to take into account the inputScale
        .derivative = true,
      }},
    {"relu_p",
      FunctionData{
        .name = "ReLU_P",
        .longName = "Derivative Rectified Linear Unit",
        .formula = "1/(1+exp(-1b256*X))",  // Here we use a quasi-threshold function
        .fun = ReLU_P,                     // enum
        .signedOut = false,                // output unsigned
        .slightRescale = true,             // output touches 1 and needs to be slightly rescaled
        .scaleFactor = 1.0,                // Function is a derivative, so we need to take into account the inputScale
        .derivative = true,
      }},
    {"elu_p",
      FunctionData{
        .name = "ELU_P",
        .longName = "Derivative Exponential Linear Unit",
        .formula = "1/(1+exp(-1b256*X))+exp(X)*(1-1/(1+exp(-1b256*X)))",  // Here we use a quasi-threshold function
        .fun = ELU_P,                                                     // enum
        .signedOut = false,                                               // output unsigned
        .slightRescale = true,                                            // output touches 1 and needs to be slightly rescaled
        .scaleFactor = 1.0,                                               // Function is a derivative, so we need to take into account the inputScale
        .derivative = true,
      }},
    {"silu_p",
      FunctionData{
        .name = "SiLU_P",
        .longName = "Sigmoid Linear Unit",
        .formula = "(1+X*exp(-X)+exp(-X))/(1+exp(-X))^2",  // textbook
        .fun = SiLU_P,                                     // enum
        .signedOut = true,                                 // output signed
        .scaleFactor = 0x1.198f14p0,                       // Function is a derivative, so we need to take into account the inputScale
        .derivative = true,
      }},
    {"gelu_p",
      FunctionData{
        .name = "GeLU_P",
        .longName = "Gaussian Error Linear Unit",
        .formula = "(X*exp(-(X^2)/2))/sqrt(2*pi)+(1+erf(X/sqrt(2)))/2",  // textbook
        .fun = GeLU_P,                                                   // enum
        .signedOut = true,                                               // output signed
        .scaleFactor = 0x1.20fffp0,                                      // Function is a derivative, so we need to take into account the inputScale
        .derivative = true,
      }},
  };


  enum Method { PlainTable, MultiPartite, Horner, PiecewiseHorner2, PiecewiseHorner3, Auto };
  static const map<string, Method> methodMap = {
    {"plaintable", PlainTable},
    {"multipartite", MultiPartite},
    {"horner", Horner},
    {"piecewisehorner2", PiecewiseHorner2},
    {"piecewisehorner3", PiecewiseHorner3},
    {"auto", Auto},
  };

  static inline const string methodOperator(Method m)
  {
    switch(m) {
    case PlainTable:
      return "FixFunctionByTable";
    case MultiPartite:
      return "FixFunctionByMultipartiteTable";
    case Horner:
      return "FixFunctionBySimplePoly";
    case PiecewiseHorner2:
    case PiecewiseHorner3:
      return "FixFunctionByPiecewisePoly";
    default:
      throw("Unsupported method.");
    };
  };

  static const Method defaultMethod = PlainTable;

  SNAFU::SNAFU(OperatorPtr parentOp_,
    Target* target_,
    string fIn,
    int wIn_,
    int wOut_,
    string methodIn,
    double inputScale_,
    int adhocCompression_,
    bool expensiveSymmetry_)
      : Operator(parentOp_, target_), wIn(wIn_), wOut(wOut_), inputScale(inputScale_), adhocCompression(adhocCompression_),
        expensiveSymmetry(expensiveSymmetry_)
  {
    if(inputScale <= 0) {
      throw(string("inputScale should be strictly positive"));
    }

    Method method;
    try {
      method = methodMap.at(to_lowercase(methodIn));
    } catch(const std::out_of_range&) {
      throw("the method '" + methodIn + "' is unknown");
    }

    string* function;  // Points to the function we need to approximate

    FunctionData af = activationFunction.at(toLowerCase(fIn));

    //determine the LSB of the input and output
    int lsbIn = -(wIn - 1);  // -1 for the sign bit
    int lsbOut;              // will be set below, it depends on signedness and if the function is a relu variant

    size_t in = 0;

    // Map of parameters
    map<string, string> params = {};

    ///////// Ad-hoc compression consists, in the ReLU variants, to subtract ReLU
    if(1 == adhocCompression && !af.reluVariant) {  // the user asked to compress a function that is not ReLU-like
      REPORT(LogLevel::MESSAGE,
        ""
          << " ??????? You asked for the compression of " << af.longName << " which is not a ReLU-like" << endl
          << " ??????? I will try but I am doubtful about the result." << endl
          << " Proceeeding nevertheless..." << lsbOut);
    }

    if(-1 == adhocCompression) {  // means "automatic" and it is the default
      adhocCompression = af.reluVariant;
    }                             // otherwise respect it.


    //////////////   Function preprocessing, for a good hardware match

    // scale the function by replacing X with (inputScale*X)
    // string replacement of "x" is dangerous because it may appear in the name of functions, so use @
    string scaleString = "(" + to_string(inputScale) + "*@)";

    string sollyaFunction = replaceX(af.formula, scaleString);
    string sollyaReLU = replaceX(activationFunction.at("relu").formula, scaleString);

    // Compute lsbOut from wOut and the constraints inferred by the function
    lsbOut = -wOut + af.signedOut;

    if(af.scaleFactor != 0.0) {
      int e;

      // Recover the exponent and fraction
      double fr = frexp(inputScale * af.scaleFactor, &e);

      // No output should touch exactly one,
      // so 2^n only needs n bits to be reprensented
      if(fr == 0.5) e--;

      // We only reduce precision when the output can get bigger than 1
      if(e > 0) lsbOut += e;
    }

    if(af.slightRescale) {
      sollyaFunction = "(1-1b" + to_string(lsbOut) + ")*(" + sollyaFunction + ")";
      sollyaReLU = "(1-1b" + to_string(lsbOut) + ")*(" + sollyaReLU + ")";
    }

    if(af.derivative) {
      sollyaFunction = to_string(inputScale) + "*(" + sollyaFunction + ")";
      // No derivative is a ReLU variant, so no need to modify the string
    }

    params["lsbOut"] = to_string(lsbOut);

    bool signedIn = true;

    // Only then we may define the delta function
    if(adhocCompression) {
      if(af.fun == ELU) {
        // We need to compute on the negative values only
        sollyaDeltaFunction = "-(" + replaceX(sollyaFunction, "-@") + ")";
      } else {
        sollyaDeltaFunction = "(" + sollyaReLU + ")-(" + sollyaFunction + ")";
      }

      function = &sollyaDeltaFunction;
      signedIn = false;  // We can exploit symetry on all the delta functions
    } else {
      function = &sollyaFunction;
    }

    params["signedIn"] = to_string(signedIn);

    // means "please choose for me"
    if(method == Auto) {
      // TODO a complex sequence of if then else once the experiments are done
      method = PlainTable;
    }

    REPORT(LogLevel::MESSAGE,
      "Function after pre-processing: " << af.longName << " evaluated on [-1,1)" << endl
                                        << " wIn=" << wIn << " translates to lsbIn=" << lsbIn << endl
                                        << " wOut=" << wOut << " translates to lsbOut=" << lsbOut);
    REPORT(LogLevel::MESSAGE, "Method is " << methodIn);

    if(adhocCompression) {
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
    name << af.name << "_" << wIn << "_" << wOut << "_" << method;
    setNameWithFreqAndUID(name.str());

    setCopyrightString("Florent de Dinechin (2023)");

    addInput(input(in), wIn);
    addOutput("Y", wOut);

    if(af.fun == ReLU) {
      // Special case for ReLU
      // TODO: Take into account rounding if necessary
      vhdl << tab << "Y <= " << relu(wIn, wOut);
      return;
    }

    if(adhocCompression) {
      if(wIn != wOut) {
        throw(string("Too lazy so far to support wIn<>wOut in case of ad-hoc compression "));
      };

      vhdl << tab << declare("ReLU", wOut) << " <= " << relu(wIn, wOut);
    }

    switch(method) {
    case PlainTable: {
      addComment("This function is correctly rounded");
      correctlyRounded = true;
      break;
    }
    case MultiPartite: {
      break;
    }
    case Horner: {
      // When compressing, we only deal with the positive interval so we rescale to the whole interval
      if(adhocCompression) {
        *function = replaceX(*function, "((@+1)/2)");
        lsbIn++;                   // The scaling means that we shift the fixed value
        params["signedIn"] = "1";  // Force the input to be signed, it will be scaled accordingly
      }
      break;
    }
    case PiecewiseHorner2: {
      params["d"] = "2";
      break;
    }
    case PiecewiseHorner3: {
      params["d"] = "3";
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

      if(method == Horner) {
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
      af.name + (adhocCompression ? "_delta_SNAFU" : "_SNAFU"),
      paramString,
      "X => " + input(in),
      adhocCompression ? "Y => D" : "Y => F");

    if(adhocCompression) {
      // Reconstruct the function
      int deltaOutSize = getSignalByName("D")->width();

      if(af.fun == ELU) {
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
