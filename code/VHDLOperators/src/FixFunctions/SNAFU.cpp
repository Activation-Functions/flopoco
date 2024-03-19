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

#include <iostream>

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

namespace flopoco
{

  struct ActivationFunctionData {
    string stdShortName;
    string fullDescription;
    string sollyaString;
    bool signedOutput;
    bool needsSlightRescale;  // for output range efficiency of functions that touch 1
    bool reluVariant;
  };

  static const map<string, ActivationFunctionData> activationFunction = {
    // two annoyances below:
    // 1/ we are not allowed spaces because it would mess the argument parser. Silly
    // 2/ no if then else in sollya, so we emulate with the quasi-threshold function 1/(1+exp(-1b256*X))
    {"sigmoid",
      {
        "Sigmoid",
        "Sigmoid",
        "1/(1+exp(-X))",  //textbook
        false,            // output unsigned
        true,             // output touches 1 and needs to be slightly rescaled
        false             // not a ReLU variant
      }},
    {"tanh",
      {
        "TanH",
        "Hyperbolic Tangent",
        "tanh(X)",  //textbook
        true,       // output signed
        true,       // output touches 1 and needs to be slightly rescaled
        false       // not a ReLU variant
      }},
    {"relu",
      {
        "ReLU",
        "Rectified Linear Unit",
        "X/(1+exp(-1b256*X))",  // Here we use a quasi-threshold function
        false,                  // output unsigned
        false,                  // output does not need rescaling -- TODO if win>wout it probably does
        false                   // ReLU variant
      }},
    {"elu",
      {
        "ELU",
        "Exponential Linear Unit",
        "X/(1+exp(-1b256*X))+expm1(X)*(1-1/(1+exp(-1b256*X)))",  // Here we use a quasi-threshold function
        true,                                                    // output signed
        false,                                                   // output does not need rescaling -- TODO CHECK
        true                                                     // ReLU variant
      }},
    {"silu",
      {
        "SiLU",
        "Sigmoid Linear Unit",
        "X/(1+exp(-X))",  // textbook
        true,             // output signed
        false,            // output does not need rescaling -- TODO CHECK
        true              // ReLU variant
      }},
    {"gelu",
      {
        "GeLU",
        "Gaussian Error Linear Unit",
        "X/2*(1+erf(X/sqrt(2)))",  // textbook
        true,                      // output signed
        false,                     // output does not need rescaling -- TODO CHECK
        true                       // ReLU variant
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

  SNAFU::SNAFU(OperatorPtr parentOp_, Target* target_, string fIn, int wIn_, int wOut_, string methodIn, double inputScale_, int adhocCompression_)
      : Operator(parentOp_, target_), wIn(wIn_), wOut(wOut_), inputScale(inputScale_), adhocCompression(adhocCompression_)
  {
    if(inputScale <= 0) {
      throw(string("inputScale should be strictly positive"));
    }

    string fl = toLowerCase(fIn);
    Method method;
    try {
      method = methodMap.at(to_lowercase(methodIn));
    } catch(const std::out_of_range&) {
      throw("the method '" + methodIn + "' is unknown");
    }

    string* function;  // Points to the function we need to approximate

    auto af = activationFunction.at(fl);
    string sollyaFunction = af.sollyaString;
    string functionName = af.stdShortName;
    string functionDescription = af.fullDescription;
    bool signedOutput = af.signedOutput;
    bool needsSlightRescale = af.needsSlightRescale;  // for output range efficiency of functions that touch 1

    string sollyaReLU = activationFunction.at("relu").sollyaString;

    //determine the LSB of the input and output
    int lsbIn = -(wIn - 1);  // -1 for the sign bit
    int lsbOut;              // will be set below, it depends on signedness and if the function is a relu variant


    ///////// Ad-hoc compression consists, in the ReLU variants, to subtract ReLU
    if(1 == adhocCompression && !af.reluVariant) {  // the user asked to compress a function that is not ReLU-like
      REPORT(LogLevel::MESSAGE,
        ""
          << " ??????? You asked for the compression of " << functionDescription << " which is not a ReLU-like" << endl
          << " ??????? I will try but I am doubtful about the result." << endl
          << " Proceeeding nevertheless..." << lsbOut);
    }

    if(-1 == adhocCompression) {  // means "automatic" and it is the default
      adhocCompression = af.reluVariant;
    }                             // otherwise respect it.


    //////////////   Function preprocessing, for a good hardware match

    // scale the function by replacing X with (inputScale*x)
    // string replacement of "x" is dangerous because it may appear in the name of functions, so use @
    string replaceString = "(" + to_string(inputScale) + "*@)";
    sollyaFunction = _replace(sollyaFunction, "X", replaceString);
    sollyaFunction = _replace(sollyaFunction, "@", "X");

    // do the same for the relu
    sollyaReLU = _replace(sollyaReLU, "X", replaceString);
    sollyaReLU = _replace(sollyaReLU, "@", "X");

    // if necessary scale the output so the value 1 is never reached
    if(needsSlightRescale) {
      lsbOut = -wOut + signedOutput;
      sollyaFunction = "(1-1b" + to_string(lsbOut) + ")*(" + sollyaFunction + ")";
      sollyaReLU = "(1-1b" + to_string(lsbOut) + ")*(" + sollyaReLU + ")";
    } else if(af.reluVariant || fl == "relu") {
      lsbOut = -wOut + intlog2(inputScale);
    } else {
      throw(string("unable to set lsbOut"));
    }

    bool signedIn = true;

    // Only then we may define the delta function
    if(adhocCompression) {
      sollyaDeltaFunction = "(" + sollyaReLU + ")-(" + sollyaFunction + ")";
      function = &sollyaDeltaFunction;
      signedIn = false;  // We can exploit symetry on all the delta functions
    } else {
      function = &sollyaFunction;
    }

    // means "please choose for me"
    if(method == Auto) {
      // TODO a complex sequence of if then else once the experiments are done
      method = PlainTable;
    }

    REPORT(LogLevel::MESSAGE,
      "Function after pre-processing: " << functionDescription << " evaluated on [-1,1)" << endl
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

    bool correctlyRounded = false;  // default is faithful

    f = new FixFunction(sollyaFunction, true, lsbIn, lsbOut);

    ostringstream name;
    name << functionName << "_" << wIn << "_" << wOut << "_" << method;
    setNameWithFreqAndUID(name.str());

    setCopyrightString("Florent de Dinechin (2023)");

    addInput("X", wIn);
    addOutput("Y", wOut);

    if(fl == "relu") {
      // Special case for ReLU
      // TODO: Take into account rounding if necessary
      vhdl << tab << "Y <= " << relu(wIn, wOut);
      return;
    }

    // Base parameters
    string paramString = join("f=", *function) + join(" lsbIn=", lsbIn) + join(" lsbOut=", lsbOut) + join(" signedIn=", signedIn);

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
      break;
    }
    case PiecewiseHorner2: {
      paramString += " d=2";
      break;
    }
    case PiecewiseHorner3: {
      paramString += " d=3";
      break;
    }
    default:
      throw(string("Method: ") + methodIn + " currently unsupported");
      break;
    };

    if(!signedIn) {
      // Compute the absolute value of X
      vhdl << tab << declare("AX", wIn) << " <= (not(X) + (" << zg(wIn - 1) << " & '1')) when X" << of(wIn - 1) << " = '1' else X;" << endl;
      vhdl << tab << declare("DX", wIn - 1) << " <= AX" << range(wIn - 2, 0) << ";" << endl;
    }

    OperatorPtr op = newInstance(methodOperator(method),
      functionName + (adhocCompression ? "_delta_SNAFU" : "_SNAFU"),
      paramString,
      signedIn ? "X=>X" : "X => DX",
      adhocCompression ? "Y => D" : "Y => F");

    if(adhocCompression) {
      // Reconstruct the function
      int deltaOutSize = getSignalByName("D")->width();
      // if all went well deltaOut should have fewer bits so we need to pad with zeroes
      vhdl << tab << declare("F", wOut) << " <= ReLU - (" << zg(wOut - deltaOutSize) << " & D);" << endl;
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
    emulate_fixfunction(*f, tc, true /* correct rounding */);
  }

  OperatorPtr SNAFU::parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args, UserInterface& ui)
  {
    int wIn, wOut;
    double inputScale;
    string fIn, methodIn;
    int adhocCompression;
    ui.parseString(args, "f", &fIn);
    ui.parseInt(args, "wIn", &wIn);
    ui.parseInt(args, "wOut", &wOut);
    ui.parseString(args, "method", &methodIn);
    ui.parseFloat(args, "inputScale", &inputScale);
    ui.parseInt(args, "adhocCompression", &adhocCompression);
    return new SNAFU(parentOp, target, fIn, wIn, wOut, methodIn, inputScale, adhocCompression);
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
    "inputScale(real)=8.0: the input scaling factor: the 2^wIn input values are mapped on the interval[-inputScale, inputScale) ; "
    "method(string)=auto: approximation method, among \"PlainTable\",\"MultiPartite\", \"Horner\", \"PiecewiseHorner2\", \"PiecewiseHorner3\", "
    "\"auto\" ;"
    "adhocCompression(int)=-1: 1: subtract the base function ReLU to implement only the non-linear part. 0: do nothing. -1: automatic;",
    "",
  };


}  // namespace flopoco
