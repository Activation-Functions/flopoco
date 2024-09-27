/*

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL, INSA,
  2008-2014.
  All rights reserved.

*/
#pragma once

#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/Operator.hpp"

#include <unordered_set>
#include <vector>

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
  InvExp,
  InvExp_P,  
};

static const map<string, ActivationFunction> functionMap = {
  {"sigmoid", Sigmoid},
  {"sigmoid_p", Sigmoid_P},
  {"tanh", TanH},
  {"tanh_p", TanH_P},
  {"relu", ReLU},
  {"relu_p", ReLU_P},
  {"gelu", GeLU},
  {"gelu_p", GeLU_P},
  {"elu", ELU},
  {"elu_p", ELU_P},
  {"silu", SiLU},
  {"silu_p", SiLU_P},
  {"invexp", InvExp},
  {"invexp_p", InvExp_P},
};

static inline const ActivationFunction functionFromString(const string s)
{
  try {
    return functionMap.at(toLowerCase(s));
  } catch(const std::out_of_range&) {
    string e = "Unknown function: " + s + "\nPossible values are: ";

    for(const auto [v, _]: functionMap) {
      e += v + " ";
    }

    throw(e);
  }
}

enum class Method {
  Auto,
  PlainTable,
  MultiPartite,
  Horner,
  PiecewiseHorner2,
  PiecewiseHorner3,
};

static const map<string, Method> methodMap = {
  {"plaintable", Method::PlainTable},
  {"multipartite", Method::MultiPartite},
  {"horner", Method::Horner},
  {"piecewisehorner2", Method::PiecewiseHorner2},
  {"piecewisehorner3", Method::PiecewiseHorner3},
  {"auto", Method::Auto},
};

static inline const Method methodFromString(const string s)
{
  try {
    return methodMap.at(toLowerCase(s));
  } catch(const std::out_of_range&) {
    string e = "Unknown method: " + s + "\nPossible values are: ";

    for(const auto [v, _]: methodMap) {
      e += v + " ";
    }

    throw(e);
  }
}

static inline const string methodOperator(Method m)
{
  switch(m) {
  case Method::PlainTable:
    return "FixFunctionByTable";
  case Method::Horner:
    return "FixFunctionBySimplePoly";
  case Method::MultiPartite:
    // return "FixFunctionByMultipartiteTable";
  case Method::PiecewiseHorner2:
  case Method::PiecewiseHorner3:
    return "FixFunctionByPiecewisePoly";
  default:
    throw("Unsupported method.");
  };
};

static const Method defaultMethod = Method::PlainTable;

enum class Compression : int {
  Auto = -1,
  Disabled = 0,
  Enabled = 1,
};

enum class Parity {
  None,
  Odd,
  Even,
};

enum class Delta {
  None,
  ReLU,
  ReLU_P,
};

struct FunctionData {
  string name;
  string longName;
  string formula;
  bool signedOut;
  Parity parity = Parity::None;       // No assumption is made on the parity, when a deltaFunction is given,
                                      // the parity is considered after teh difference to this function

  Delta deltaFunction = Delta::None;  // The function to substract for compression
  bool signedDelta = false;           // Wether the delta-reduced function has a signed output
  double offset = 0.0;                // The offset
  bool slightRescale = false;         // for output range efficiency of functions that touch 1
  double scaleFactor = 0.0;           // The importancee of tne inputScale
  bool derivative = false;            // Whether the function is a derivative, and we need to multiply the output by the inputScale
  unordered_set<Method> incompatibleMethods = {};
};

static const map<ActivationFunction, FunctionData> activationFunction = {
  // two annoyances below:
  // 1/ we are not allowed spaces because it would mess the argument parser. Silly
  // 2/ no if then else in sollya, so we emulate with the quasi-threshold function 1/(1+exp(-1b256*X))
  {Sigmoid,
    FunctionData{
      .name = "Sigmoid",
      .longName = "Sigmoid",
      .formula = "1/(1+exp(-X))",  //textbook
      .signedOut = false,          // output unsigned
      .parity = Parity::Odd,
      .offset = 0.5,
      .slightRescale = true,       // output touches 1 and needs to be slightly rescaled
    }},
  {TanH,
    FunctionData{
      .name = "TanH",
      .longName = "Hyperbolic Tangent",
      .formula = "tanh(X)",
      .signedOut = true,                        // output signed
      .parity = Parity::Odd,
      .slightRescale = true,                    // output touches 1 and needs to be slightly rescaled
      .incompatibleMethods = {Method::Horner},  // Simple Horner will not work
    }},
  {ReLU,
    FunctionData{
      .name = "ReLU",
      .longName = "Rectified Linear Unit",
      .formula = "X/(1+exp(-1b256*X))",  // Here we use a quasi-threshold function
      .signedOut = false,                // output unsigned
      .scaleFactor = 1.0,                //
    }},
  {ELU,
    FunctionData{
      .name = "ELU",
      .longName = "Exponential Linear Unit",
      .formula = "X/(1+exp(-1b256*X))+expm1(X)*(1-1/(1+exp(-1b256*X)))",  // Here we use a quasi-threshold function
      .signedOut = true,                                                  // output signed
      .deltaFunction = Delta::ReLU,
      .scaleFactor = 1.0,                                                 //
    }},
  {SiLU,
    FunctionData{
      .name = "SiLU",
      .longName = "Sigmoid Linear Unit",
      .formula = "X/(1+exp(-X))",  // textbook
      .signedOut = true,           // output signed
      .parity = Parity::Even,
      .deltaFunction = Delta::ReLU,
      .scaleFactor = 1.0,          //
    }},
  {GeLU,
    FunctionData{
      .name = "GeLU",
      .longName = "Gaussian Error Linear Unit",
      .formula = "(X/2)*(1+erf(X/sqrt(2)))",  // textbook
      .signedOut = true,                      // output signed
      .parity = Parity::Even,
      .deltaFunction = Delta::ReLU,
      .scaleFactor = 1.0,                     //
    }},
  {InvExp,
    FunctionData{
      .name = "InvExp",
      .longName = "Inverse Exponential",
      .formula = "exp(-X)",  // textbook
      .signedOut = false,           // output unsigned
      .parity = Parity::None,
      .scaleFactor = 1.0,          //
    }},

  // Derivatives
  {Sigmoid_P,
    FunctionData{
      .name = "Sigmoid_P",
      .longName = "Derivative Sigmoid",
      .formula = "exp(-X)/(1+exp(-X))^2",  //textbook
      .signedOut = false,                  // output unsigned
      .parity = Parity::Even,
      .slightRescale = true,
      .scaleFactor = 0.25,
      .derivative = true,
    }},
  {TanH_P,
    FunctionData{
      .name = "TanH_P",
      .longName = "Derivative Hyperbolic Tangent",
      .formula = "(1-tanh(X)^2)",  //textbook
      .signedOut = false,          // output unsigned
      .parity = Parity::Even,
      .slightRescale = true,       // output touches 1 and needs to be slightly rescaled
      .scaleFactor = 1.0,          // Function is a derivative, so we need to take into account the inputScale
      .derivative = true,
    }},
  {ReLU_P,
    FunctionData{
      .name = "ReLU_P",
      .longName = "Derivative Rectified Linear Unit",
      .formula = "1/(1+exp(-1b256*(X+1b-5)))",  // Here we use a quasi-threshold function
      .signedOut = false,                       // output unsigned
      .slightRescale = true,                    // output touches 1 and needs to be slightly rescaled
      .scaleFactor = 1.0,                       // Function is a derivative, so we need to take into account the inputScale
      .derivative = true,
    }},
  {ELU_P,
    FunctionData{
      .name = "ELU_P",
      .longName = "Derivative Exponential Linear Unit",
      .formula = "1/(1+exp(-1b256*X))+exp(X)*(1-1/(1+exp(-1b256*X)))",  // Here we use a quasi-threshold function
      .signedOut = false,                                               // output unsigned
      .slightRescale = true,                                            // output touches 1 and needs to be slightly rescaled
      .scaleFactor = 1.0,                                               // Function is a derivative, so we need to take into account the inputScale
      .derivative = true,
    }},
  {SiLU_P,
    FunctionData{
      .name = "SiLU_P",
      .longName = "Sigmoid Linear Unit",
      .formula = "(1+X*exp(-X)+exp(-X))/(1+exp(-X))^2",  // textbook
      .signedOut = true,                                 // output signed
      .parity = Parity::Odd,
      .deltaFunction = Delta::ReLU_P,
      .signedDelta = true,
      .scaleFactor = 0x1.198f14p0,  // Function is a derivative, so we need to take into account the inputScale
      .derivative = true,
    }},
  {GeLU_P,
    FunctionData{
      .name = "GeLU_P",
      .longName = "Gaussian Error Linear Unit",
      .formula = "(X*exp(-(X^2)/2))/sqrt(2*pi)+(1+erf(X/sqrt(2)))/2",  // textbook
      .signedOut = true,                                               // output signed
      .parity = Parity::Odd,
      .deltaFunction = Delta::ReLU_P,
      .signedDelta = true,
      .scaleFactor = 0x1.20fffp0,  // Function is a derivative, so we need to take into account the inputScale
      .derivative = true,
    }},
  {InvExp_P, // This is probably stupid to have
    FunctionData{
      .name = "InvExp",
      .longName = "Inverse Exponential",
      .formula = "-exp(-X)",  // textbook
      .signedOut = true,           // output signed (always negative)
      .parity = Parity::None,
      .scaleFactor = 1.0,          //
    }},
};

namespace flopoco
{
  /**
   * A generator of Simple Neural Actication Function Units (SNAFU) without reinventing the wheel
	 */

  class SNAFU : public Operator {
    public:
    SNAFU(OperatorPtr parentOp,
      Target* target,
      string f,
      int wIn,
      int wOut,
      string method,
      double inputScale,
      int adhocCompression,
      bool expensiveSymmetry);

    void emulate(TestCase* tc);

    static OperatorPtr parseArguments(OperatorPtr parentOp, Target* target, vector<string>& args, UserInterface& ui);
    private:
    int wIn;
    int wOut;
    double inputScale;
    Compression adhocCompression;
    bool expensiveSymmetry;
    string sollyaDeltaFunction;  // when we use compression
    FixFunction* f;
    bool correctlyRounded;
  };

}  // namespace flopoco
