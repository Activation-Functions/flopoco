#ifndef _FIXFUNCTION_HPP_
#define _FIXFUNCTION_HPP_

#include <gmpxx.h>
#include <iostream>
#include <sollya.h>
#include <string>

/* Stylistic convention here: all the sollya_obj_t have names that end with a capital S */
namespace flopoco
{

  /** The FixFunction objects holds a fixed-point function. 
			It provides an interface to Sollya services such as 
			parsing it,
			evaluating it at arbitrary precision,
			providing a polynomial approximation on an interval
	*/

  class FixFunction {
    public:
    /**
		 A FixFunctionByTable constructor when the precision parameters are already known
			@param[string] func    a string representing the function
			@param[bool]   signedIn: if true, input range is [0,1], else input range is [0,1]
			@param[int]    lsbIn    input LSB weight (-lsbX is the input size)
			@param[int]    lsbOut  output LSB weight
			Default value of -1717 correspond to a function for which the actuak lsbs are unknown yet.
		*/
    FixFunction(std::string sollyaString, bool signedIn, int lsbIn = -1717, int lsbOut = -1717);

    /**
		 A FixFunctionByTable constructor from a Sollya object
			@param[sollya_obj_t] fS: the function provided as a Sollya expression
			@param[bool]   signedIn: if true, input range is [0,1], else input range is [0,1]
			@param[int]    lsbIn    input LSB weight (-lsbX is the input size)
			@param[int]    lsbOut  output LSB weight
			Default value of -1717 correspond to a function for which the actuak lsbs are unknown yet.
		*/
    FixFunction(sollya_obj_t fS, bool signedIn, int lsbIn = -1717, int lsbOut = -1717);

    virtual ~FixFunction();

    std::string getDescription() const;

    /** helper method: computes the value of the function on a double; no range check */
    double eval(double x) const;

    /** helper method: computes the value of the function on an MPFR; no range check */
    void eval(mpfr_t r, mpfr_t x) const;

    /** if correctlyRounded is true, compute the CR result in rNorD; otherwise computes the RD in rNorD and the RU in ru.
			x as well as the result(s) are given as bit vectors.
			These bit vectors are held in a mpz_class which is always positive, although it may hold a two's complement value.
			(this is the usual behaviour of emulate())
	*/
    void eval(mpz_class x, mpz_class& rNorD, mpz_class& ru, bool correctlyRounded = false) const;

    // All the following public, not good practice I know, but life is complicated enough
    // All these public attributes are read at some point by Operator classes
    std::string sollyaString;
    int lsbIn;  /*< A value of -1717 is used for an unitialized lsb, so let's be ultra accurate. Ugly but hopefully harmless. */
    int wIn;
    int msbOut; /*< Now computed by the constructor; assuming signed out */
    int lsbOut;
    int wOut;
    bool signedIn;
    bool signedOut;           /**< computed by the constructor */
    std::string description;
    sollya_obj_t fS;
    sollya_obj_t inputRangeS; /**< computed by the constructor */
    private:
    void initialize();
    std::string outputDescription;
  };

}  // namespace flopoco
#endif  // _FIXFUNCTION_HH_
