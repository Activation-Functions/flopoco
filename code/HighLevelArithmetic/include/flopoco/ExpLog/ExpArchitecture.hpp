/*
  Helper to chose the architecture for FP exponential for FloPoCo

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Author : Florent de Dinechin, Florent.de-Dinechin@insa-lyon.fr

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2010.
  All rights reserved.

*/
#ifndef __EXPARCH_HPP
#define __EXPARCH_HPP
#include <sstream>
#include <vector>

#include "flopoco/utils.hpp"

class Fragment;


namespace flopoco{


	class ExpArchitecture
	{
	public:

		/** @brief The constructor with manual control of all options
		* @param wE exponent size
		* @param wF fraction size
		* @param k size of the input to the first table
		* @param d  degree of the polynomial approximation (if k=d=0, the
		* 			constructor tries to compute sensible values)
		* @param guardBits number of gard bits. If -1, a default value (that
		* 				   depends of the size)  is computed inside the constructor.
		* @param fullInput boolean, if true input mantissa is of size wE+wF+1,
		*                  so that  input shift doesn't padd it with 0s (useful
		*                  for FPPow)
		*/
		ExpArchitecture(
					int blockRAMSize,
					int wE,
					int wF,
					int k,
					int d,
					int guardBits=-1,
					bool fullInput=false,
					bool IEEEFPMode=false);

		~ExpArchitecture();

		int getd();
		int getg();
		int getk();
		int getMSB();
		int getLSB();
		bool getExpYTabulated();
		bool getUseTableExpZm1(); /**< Use  table to approximate expZ-1 */
		bool getUseTableExpZmZm1(); /**< Use table to approximate expZ-Z-1 */
		bool getIEEEFPMode();
		int getSizeY(); /**< Size of input */
		int getSizeZ(); /**< Size of the tabulated part of the input */
		int getSizeExpY(); /**< Size of the exponential of input*/
		int getSizeExpA(); /**< Size of the exponential of tabulated */
		int getSizeZtrunc(); /**< Input size of table expZ-Z-1 */
		int getSizeExpZmZm1(); /**< Output size of table expZ-Z-1 */
		int getSizeExpZm1(); /**< Output size of table expZ-1 */
		int getSizeMultIn(); // sacrificing accuracy where it costs		

	private:
		int wE; /**< Exponent size */
		int wF; /**< Fraction size */
		int MSB; /**< Max MSB of fixpoint input for the chosen output format */
		int LSB; /**< Min LSB of fixpoint input for the chosen output format */
		int k;  /**< Size of the address bits for the first table  */
		int d;  /**< Degree of the polynomial approximation */
		int g;  /**< Number of guard bits */
		// Various architecture parameter to be determined before attempting to
		// build the architecture
		bool expYTabulated; /**< Is Y fully tabulated */
		bool useTableExpZm1; /**< Use  table to approximate expZ-1 */
		bool useTableExpZmZm1; /**< Use table to approximate expZ-Z-1 */
		bool IEEEFPMode; /** Using IEEE mode, need to know for subnormals*/
		int sizeY; /**< Size of input */
		int sizeZ; /**< Size of the tabulated part of the input */
		int sizeExpY; /**< Size of the exponential of input*/
		int sizeExpA; /**< Size of the exponential of tabulated */
		int sizeZtrunc; /**< Input size of table expZ-Z-1 */
		int sizeExpZmZm1; /**< Output size of table expZ-Z-1 */
		int sizeExpZm1; /**< Output size of table expZ-1 */
		int sizeMultIn; // sacrificing accuracy where it costs
	};
}
#endif
