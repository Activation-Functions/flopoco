/*
  FixFunction object for FloPoCo

  Authors: Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Aric team at Ecole Normale Superieure de Lyon
	then by the Socrate team at INSA de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All rights reserved.

*/

#include "flopoco/FixFunctions/FixFunction.hpp"
#include "flopoco/report.hpp"

#include <cstdio>
#include <sollya.h>
#include <sstream>

namespace flopoco
{
  FixFunction::FixFunction(std::string sollyaString_, bool signedIn_, int lsbIn_, int lsbOut_)
      : sollyaString(sollyaString_), lsbIn(lsbIn_), lsbOut(lsbOut_), signedIn(signedIn_)
  {
    std::ostringstream completeDescription;
    completeDescription << sollyaString_;
    if(signedIn)
      completeDescription << " on [-1,1)";
    else
      completeDescription << " on [0,1)";

    // Now do the parsing in Sollya
    fS = sollya_lib_parse_string(sollyaString.c_str());
    /* If  parse error throw an exception */
    if(sollya_lib_obj_is_error(fS) || !sollya_lib_obj_is_function(fS))
      throw(std::string("FixFunction: Unable to parse input function: ") + sollyaString);

    initialize();

    if(lsbIn != 0) {  // we have an IO specification
      completeDescription << " for lsbIn=" << lsbIn << " (wIn=" << wIn << "), msbout=" << msbOut << ", lsbOut=" << lsbOut << " (wOut=" << wOut
                          << "). ";
    }
    completeDescription << outputDescription;
    description = completeDescription.str();
  }


  FixFunction::FixFunction(sollya_obj_t fS_, bool signedIn_, int lsbIn_, int lsbOut_) : signedIn(signedIn_), lsbIn(lsbIn_), lsbOut(lsbOut_), fS(fS_)
  {
    initialize();
  }


  void FixFunction::initialize()
  {
    if(signedIn)
      wIn = -lsbIn + 1;  // add the sign bit at position 0
    else
      wIn = -lsbIn;
#if 1                    // this sometimes enlarges the interval.
    if(signedIn)
      inputRangeS = sollya_lib_parse_string("[-1;1]");
    else
      inputRangeS = sollya_lib_parse_string("[0;1]");
#else  // This is tighter : interval is [O, 1-1b-l]  but it causes more problems than it solves
    string maxvalIn = "1-1b" + to_string(lsbIn);
    ostringstream uselessNoise;
    uselessNoise << "[" << (signedIn ? "-1" : "0") << ";" << maxvalIn << "]";
    inputRangeS = sollya_lib_parse_string(uselessNoise.str().c_str());
#endif
    sollya_obj_t outIntervalS, supS, infS;
    mpfr_t supMP, infMP, tmp;
    mpfr_init2(supMP, 1000);  // no big deal if we are not accurate here
    mpfr_init2(infMP, 1000);  // no big deal if we are not accurate here
    mpfr_init2(tmp, 1000);    // no big deal if we are not accurate here

		// TODO: Use a more intelligent method for this, using the zeroes of the derivative

		if ((wIn>0) && (wIn < 17)) {
			// Compute exhaustively the values taken by the function
			mpfr_t x, delta, r;
			mpfr_inits2(wIn, x, delta, NULL);
			mpfr_init2(r, 1000);
			mpfr_set_d(x, signedIn ? -1.0 : 0.0, MPFR_RNDN);

      // delta = 2^lsbIn
      mpfr_set_d(delta, lsbIn, MPFR_RNDN);
      mpfr_exp2(delta, delta, MPFR_RNDN);

      // Initialize the bounds
      mpfr_set_inf(supMP, -1);
      mpfr_set_inf(infMP, +1);

      // Loop
      while(mpfr_cmp_d(x, 1.0) < 0) {
        sollya_lib_evaluate_function_at_point(r, fS, x, NULL);
        mpfr_max(supMP, supMP, r, MPFR_RNDU);
        mpfr_min(infMP, infMP, r, MPFR_RNDD);

				mpfr_add(x, x, delta, MPFR_RNDN);
			}
			mpfr_clears(x,delta,r, NULL);
		} else {
			if(wIn>0) {
				std::cerr << "Warning: the minimal output width is evaluated by Sollya and may be overestimated.\n";
			}
			outIntervalS = sollya_lib_evaluate(fS,inputRangeS);
			supS = sollya_lib_sup(outIntervalS);
			infS = sollya_lib_inf(outIntervalS);
			sollya_lib_get_constant(supMP, supS);
			sollya_lib_get_constant(infMP, infS);

      sollya_lib_clear_obj(supS);
      sollya_lib_clear_obj(infS);
      sollya_lib_clear_obj(outIntervalS);
    }
    // Here we should take into account the rounding to lsbOut 
    // (we had the bug that the MPFR eval was some minuscule -epsilon wich trigerred signedOut)
    mpfr_set_d(tmp, 1.0, MPFR_RNDN); // exact
    mpfr_mul_2si(tmp, tmp, lsbOut-1, MPFR_RNDN); // half-ulp: 2^(lsbOut-1), exact
    mpfr_add(tmp, tmp, infMP, MPFR_RNDN); // infMP+one half-ulp. 
    // If this one is still negative we have a signed output

     // std::cerr << " infMP=" << mpfr_get_d(infMP, MPFR_RNDN) << "  lsbOut=" << lsbOut << std::endl;
    if(mpfr_sgn(tmp) >= 0) {
      signedOut = false;
    } else {
      signedOut = true;
    }

    std::ostringstream t;  // write it before we take the absolute value below
    t << "Out interval: [" << mpfr_get_d(infMP, MPFR_RNDD) << "; " << mpfr_get_d(supMP, MPFR_RNDU) << "]";
    //std::cerr << " " << t.str() << "   signedIn="<<signedIn << ", computed signedOut="<<signedOut <<  std::endl;
    // Now recompute the MSB explicitely.
    mpfr_abs(supMP, supMP, GMP_RNDU);
    mpfr_abs(infMP, infMP, GMP_RNDU);
    mpfr_max(tmp, infMP, supMP, GMP_RNDU);
    mpfr_log2(tmp, tmp, GMP_RNDU);
    mpfr_floor(tmp, tmp);
    msbOut = mpfr_get_si(tmp, GMP_RNDU);
    if(signedOut) msbOut++;
    //std::cerr << "Computed msbOut=" << msbOut <<std::endl;
    t << ". Output is " << (signedOut ? "signed" : "unsigned");
    outputDescription = t.str();

    mpfr_clears(supMP, infMP, tmp, NULL);

		wOut=msbOut-lsbOut+1;
#if 0 // TODO: this sanity test breaks half of the autotest of FPExp, why ?
		if(wOut<=0) {
			throw(std::string("FixFunction determined that wOut=") + std::to_string(wOut) + std::string(" which makes no sense"));
		}
#endif
	}

  FixFunction::~FixFunction()
  {
    sollya_lib_clear_obj(fS);
    sollya_lib_clear_obj(inputRangeS);
  }

  std::string FixFunction::getDescription() const
  {
    return description;
  }

  void FixFunction::eval(mpfr_t r, mpfr_t x) const
  {
    sollya_lib_evaluate_function_at_point(r, fS, x, NULL);
  }


  double FixFunction::eval(double x) const
  {
    mpfr_t mpX, mpR;
    double r;

    mpfr_inits(mpX, mpR, NULL);
    mpfr_set_d(mpX, x, GMP_RNDN);
    sollya_lib_evaluate_function_at_point(mpR, fS, mpX, NULL);
    r = mpfr_get_d(mpR, GMP_RNDN);

    mpfr_clears(mpX, mpR, NULL);
    return r;
  }


  void FixFunction::eval(mpz_class x, mpz_class& rNorD, mpz_class& ru, bool correctlyRounded) const
  {
    int precision = 100 * (wIn + wOut);
    sollya_lib_set_prec(sollya_lib_constant_from_int(precision));

    mpfr_t mpX, mpR;
    mpfr_init2(mpX, wIn + 2);
    mpfr_init2(mpR, precision);

    if(signedIn) {
      mpz_class negateBit = mpz_class(1) << (wIn);
      if((x >> (-lsbIn)) != 0) x -= negateBit;
    }
    /* Convert x to an mpfr_t in [0,1[ */
    mpfr_set_z(mpX, x.get_mpz_t(), GMP_RNDN);
    mpfr_div_2si(mpX, mpX, -lsbIn, GMP_RNDN);

    /* Compute the function */
    eval(mpR, mpX);
    //		REPORT(LogLevel::FULL,"function() input is:"<<sPrintBinary(mpX));
    //cerr << 100*(wIn+wOut) <<" function("<<mpfr_get_d(mpX, GMP_RNDN)<<") output before rounding is:"<<mpfr_get_d(mpR, GMP_RNDN) << " " ;
    /* Compute the signal value */
    mpfr_mul_2si(mpR, mpR, -lsbOut, GMP_RNDN);

    /* So far we have a highly accurate evaluation. Rounding to target size happens only now
		 */
    if(correctlyRounded) {
      mpfr_get_z(rNorD.get_mpz_t(), mpR, GMP_RNDN);
      // convert to two's complement
      if(rNorD < 0) {
        rNorD += (mpz_class(1) << wOut);
      }

    } else {
      mpfr_get_z(rNorD.get_mpz_t(), mpR, GMP_RNDD);
      if(rNorD < 0) {
        rNorD += (mpz_class(1) << (wOut));
      }
      mpfr_get_z(ru.get_mpz_t(), mpR, GMP_RNDU);
      if(ru < 0) {
        ru += (mpz_class(1) << wOut);
      }
    }

    //		REPORT(LogLevel::FULL,"function() output r = ["<<rd<<", " << ru << "]");
    mpfr_clear(mpX);
    mpfr_clear(mpR);
  }
}  // namespace flopoco
