#ifndef PIF2POSIT_HPP
#define PIF2POSIT_HPP


/* This file contains a lot of useful functions to manipulate vhdl */
#include "flopoco/utils.hpp"

/* Each Operator declared within the flopoco framework has 
   to inherit the class Operator and overload some functions listed below*/
#include "flopoco/Operator.hpp"

/*  All flopoco operators and utility functions are declared within
    the flopoco namespace.
    You have to use flopoco:: or using namespace flopoco in order to access these
    functions.
*/

namespace flopoco {
  
  // new operator class declaration
  class PIF2Posit : public Operator {
    
  public:        
    PIF2Posit(Target* target, Operator* parentOp, int widthO, int wES);
    
    // destructor
    // ~PIF2Posit();
    
    void emulate(TestCase * tc);
    static void computePIFWidths(int const widthO, int const wES, int* wE, int* wF);
    
    
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args);
    
    
    static void registerFactory();
    
  private:
    int wE_;
    int wF_;
    int widthO_;
    int wES_;
    
  };
  
  
}//namespace

#endif
