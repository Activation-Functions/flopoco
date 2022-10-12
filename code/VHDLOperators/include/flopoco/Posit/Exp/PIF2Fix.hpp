#ifndef PIF2FIX_HPP
#define PIF2FIX_HPP

#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/utils.hpp"

namespace flopoco {
  
  // new operator class declaration
  class PIF2Fix : public Operator {
    
  public:        
    PIF2Fix(Target* target, Operator* parentOp, int widthP= 32, int wES = 2);
    
    // destructor
    // ~PIF2Posit();
    
    void emulate(TestCase * tc);
    static void computePIFWidths(int const widthP, int const wES, int* wE, int* wF);
    
    
    static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

  private:
    int wE_;
    int wF_;
    int widthP_;
    int wES_;
    
  };
  
  
}//namespace

#endif
