/**
  The abstract class that models different chips for delay and area.
  Classes for real chips inherit from this one. They should be in subdirectory Targets

  Authors:   Bogdan Pasca, Florent de Dinechin

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,

  All Rights Reserved


	This class unfortunately mixes up two notions:

	- features of the hardware target, in particular FPGA ones, and methods to retrieve them and / or exploit them.
	These are defined statically and users are not expected to modify them.
	Among these features, many describe the delays associated to various computations. They are intended to be used as first argument of declare() in order to build self-pipelining operators.

	- methods to set and retreive VHDL generation objectives:
    features of the generated code (e.g. do you want clock enable signals) 
	  cost/performance objectives (e.g. frequency, DSP utilization threshold, etc.) 

*/




//TODO a logicTableDelay() that would replace the delay computation in Table.cpp (start from there)
// TODO SuggestSlackAdd should add an optional parameter to suggestAdd

#ifndef TARGET_HPP
#define TARGET_HPP

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include <iostream>
#include <sstream>
#include <string>
#include <math.h>
#include "flopoco/IntMult/MultiplierBlock.hpp"

#define REPORTDELAYS false

#define TARGETREPORT(stream) {if (REPORTDELAYS){ cerr << "> " << id_ << ": " << stream << endl;}else{}} 

namespace flopoco{

	/** Abstract target Class. All classes which model real chips inherit from this class */
	class Target
	{
	public:
		/** The default constructor. Creates a pipelined target, with 4 inputs/LUT,
		 * with a desired frequency of 400MHz and which is allowed to use hardware multipliers
		 */
		Target();

		virtual ~Target();


		/** Returns ID of instantiated target. This ID is represented by the name
		 * @return the ID
		 */
		std::string getID();

		/** Returns ID of the vendor, currently "Altera" or "Xilinx" 
				because we are an old project.
		 * @return the ID
		 */
		std::string getVendor();


		/** Returns true if the target is to have pipelined design, otherwise false
		 * @return if the target is pipelined
		 */
		bool isPipelined();

		/** Returns true if the target is to have clock enable signals
		 */
		bool useClockEnable();

		void setClockEnable(bool val);

		/** Returns true if the target names signals delays with the cycle number
		 */
		bool useNameSignalByCycle();

		void setNameSignalByCycle(bool val);

		/** Returns the desired frequency for this target in Hz
		 * @return the frequency
		 */
		double frequency();

		/** Returns the desired frequency for this target in MHz
		 * @return the frequency
		 */
		double frequencyMHz();

		/** Returns the target frequency, normalized between 0 and 1 in a target-independent way.
			 1 means maximum practical frequency (400MHz on Virtex4, 500MHz on Virtex-5, etc)
			 This method is intended to make it easier to write target-independent frequency-directed operators
		 */
		double normalizedFrequency();


		/** Sets the desired frequency for this target
		 * @param f the desired frequency
		 */
		void setFrequency(double f);


		/** Returns true if the hardware target has hardware multipliers
		 */
		bool hasHardMultipliers();


		/** Returns true if the hardware target has hardware multipliers, and flopoco should use them
		 */
		bool useHardMultipliers();

		
		/** defines if flopoco should use hardware multipliers
		 */
		void setUseHardMultipliers(bool v);


		/** defines the unused hard mult threshold, see the corresponding attribute for its meaning
		 */
		void setUnusedHardMultThreshold(float v);

		/** returns the value of the unused hard mult threshold, the the corresponding attribute for its meaning
		 */
		float unusedHardMultThreshold();


		bool registerLargeTables();
		void setRegisterLargeTables(bool v);

		bool tableCompression();
		void setTableCompression(bool v);


		/** Returns true if the target has fast ternary adders in the logic blocks
		 * @return the status of the hasFastLogicTernaryAdder_ parameter
		 */

		/** defines if flopoco should produce plain stupid VHDL
		 */
		void setPlainVHDL(bool v);

		/** should flopoco produce plain stupid VHDL */
		bool plainVHDL();

		/** should flopoco generate SVG figures */
		bool generateFigures();

		/** should flopoco generate SVG figures */
		void setGenerateFigures(bool b);

		/** should target specific optimizations be performed */
		bool  useTargetOptimizations();

		/** should target specific optimizations be performed */
		void  setUseTargetOptimizations(bool b);

		/** returns the compression method used for the BitHeap */
		std::string  getCompressionMethod();

		/** sets the compression method used for the BitHeap */
		void  setCompressionMethod(std::string compression);

		/** sets the ILP solver for operators optimized by ILP. It has to match a solver name known by the ScaLP library */
		void setILPSolver(std::string ilpSolverName);

		/** returns the ILP solver */
		std::string getILPSolver();

		/** sets the timeout in seconds for the ILP solver for operators optimized by ILP.*/
		void setILPTimeout(int ilpSolverTimeout);

		/** returns the ILP solver timeout in seconds */
		int getILPTimeout();

		/** returns the compression method used for multiplier tiling */
		std::string  getTilingMethod();

		/** sets the compression method used for multiplier tiling */
		void  setTilingMethod(std::string method);

		/** On LUT-based FPGAs, number of inputs of the basic architectural LUT.
		  * Look-up tables with lutInput() input bits can be used independently
		  * sometimes with constraints that input bits are shared.
      * When the architecture of a logic bloc allows to combine 
			* several basic LUTs into larger ones, see maxLutInputs()
		  */
		int lutInputs();

		/**On LUT-based FPGAs, this is the maximum value of n such that an n-bit
		 * in, 1-bit out look-up-table can be implemented without resorting to
		 * general routing. See the documentation of this method in each each
		 * specific Target for more information
		 */
		virtual int maxLutInputs();

		/**
		 *	@brief return the number n of luts consumed for a given entry word
		 *	size. If smaller than one, it means that 1/n bits can be computed
		 *	simultaneously from the same input
		 *	Returns -1 if a function of lutInputSize cannot be implemented
		 *	without using routing in general
		 */
		virtual double lutConsumption(int lutInputSize) = 0;


		/* -------------------- Perf declaration methods  ------------------------
			 These methods return a delay in seconds.
			 Some day they will also add to the global resource consumption.

			 Currently each of the high-level logic functions include a quantum of local routing.
			 For large fanout, use fanoutDelay(fanout) on top of that

			 lutDelay is the delay of a bare lut, without any routing
		 */

		/** Logic delay of a boolean functions of the given number of inputs
				It includes one level of local routing.
				Default implementation in target uses only lutDelay etc
		*/
		virtual double logicDelay(int inputs=1) = 0;

		/** Function which returns addition delay for an n bit addition
				It includes one level of local routing.
		 * @param n the number of bits of the addition (n-bit + n-bit )
		 */
		virtual double adderDelay(int n, bool addRoutingDelay=true) =0;


		/** Function which returns addition delay for an n bit ternary addition
		 * NOTE: only relevant for architectures supporting native ternary addition
		 * @param n the number of bits of the addition (n-bit + n-bit + n-bit )
		 * @return the addition delay for an n bit ternary addition
		 */
		virtual double adder3Delay(int n) =0;

		/** Function which returns the delay for an n bit equality comparison
		* @param n the number of bits of the comparison
		* @return the delay of the comparisson between two vectors
		*/
		virtual double eqComparatorDelay(int n) =0;

		/** Function which returns the delay for an n bit "strictly greater" comparison
		* @param n the number of bits of the comparison
		* @return the delay of the comparisson between two vectors
		*/
		virtual double ltComparatorDelay(int n) =0;

		/** Function which returns the delay for an n bit equality comparison with a
		* constant
		* @param n the number of bits of the comparison
		* @return the delay of the comparisson between a vector and a constant
		*/
		virtual double eqConstComparatorDelay(int n) =0;


		// From there on:  Do not use if you can use one of the previous.
		// We would like to deprecate this level of detail.


		/**  Do not use this if you can use logicDelay
				 Function which returns the lutDelay for this target
		 * @return the LUT delay
		 */
		virtual double lutDelay() =0;

		/** Function which returns the flip-flop Delay for this target
			 (including net delay)
			 * @return the flip-flop delay
			 */
		virtual double ffDelay() =0;

		/** Function which returns the carry propagate delay
		 * @return the carry propagate delay
		 */
		virtual double carryPropagateDelay() =0;


		/** Function which returns the local wire delay (local routing)
		 * @return local wire delay
		 */
		virtual double fanoutDelay(int fanout = 1) =0;


		/* --------------------  DSP related  --------------------------------*/

		/** Function which returns the delay of the multiplier of the DSP block
		 * @return delay of the DSP multiplier
		 */
		virtual double DSPMultiplierDelay() =0;

		/** Function which returns the delay of the adder of the DSP block
		 * @return delay of the DSP adder
		 */
		virtual double DSPAdderDelay()=0;

		/** Function which returns the delay of the wiring between neighboring
		 * DSPs
		 * @return delay of the interconnect between DSPs
		 */
		virtual double DSPCascadingWireDelay()=0;

		/** Function which returns the delay of the wiring between
		 * DSPs output and logic
		 * @return delay DSP -> slice
		 */
		virtual double DSPToLogicWireDelay()=0;

 // used in Squarer
		/** Function which returns the delay of the wiring between
		 * logic and DSPs input
		 * @return delay logic -> DSP
		 */
		virtual double LogicToDSPWireDelay()=0;



		/**
		 * How many registers should be added to the pipeline after a multiplier written as "*" in VHDL
		 * hoping that these registers will be pushed inside by retiming
		 % This function is highly heuristic, as it depends on synthesis tool options that are not known
		 * @param multBlock the multiplier block representing the DSP to be added
		 * @param cycleDelay the number of cycles that need to be added
		 * @param cpDelay the delay in the critical path (needed on top of the
		 * 	@cycleDelay cycles added)
		 */
		virtual int plainMultDepth(int wX, int wY);

		/* -------------------  BRAM related  --------------------------------*/

#if 0
		/** Function which returns the delay between
		 * RAMs and slices
		 * @return delay RAMout to slice
		 */
		virtual double RAMToLogicWireDelay()=0;
#endif

		/** Function which returns the delay between
		 * slices and RAM input
		 * @return delay slice to RAMin
		 */
		virtual double LogicToRAMWireDelay()=0;
		
		/** Function which returns the delay of the RAM
		 * @return delay RAM
		 */
		virtual double RAMDelay()=0;

		/** Function which returns the delay of a table,
		 * implemented in either logic or BRAMs
		 * @param wIn the size of the input address port of the table
		 * @param wOut the size of the data output port of the table
		 */
		virtual double tableDelay(int wIn_, int wOut_, bool logicTable_);

		virtual double cycleDelay();


		/** get a vector of possible DSP configurations */
		std::vector<std::pair<int,int>> possibleDSPConfigs();

		/** get a vector telling which of the  possible DSP configurations can be used full width unsigned (typically true on Altera and false on Xilinx) */
		std::vector<bool> whichDSPCongfigCanBeUnsigned();

		
		/** Function which returns the size of a primitive memory block,which could be recognized by the synthesizer as a dual block.
		 * @return the size of the primitive memory block
		 */
		virtual long sizeOfMemoryBlock() = 0 ;


		/**
		 * How many registers should be added to the pipeline after a BRAM-based table
		 * hoping that these registers will be pushed inside by retiming
		 % This function is highly heuristic, as it depends on synthesis tool options that are not known
		 * @param multBlock the multiplier block representing the DSP to be added
		 * @param cycleDelay the number of cycles that need to be added
		 * @param cpDelay the delay in the critical path (needed on top of the
		 * 	@cycleDelay cycles added)
		 */
		virtual int tableDepth(int wIn, int wOut);

#if 0
		/** Function which returns the Equivalence between slices and a DSP.
		 * @return X ,  where X * Slices = 1 DSP
		 */
		virtual int getEquivalenceSliceDSP() = 0;
#endif

		/** Function which returns the maximum widths of the operands of a DSP
		 * @return widths with x>y
		 */
		void getMaxDSPWidths(int &x, int &y, bool sign = false);



		bool hasFastLogicTernaryAdders();

		/** Returns true if it is worth using hard multipliers for implementing a multiplier of size wX times wY */
		bool worthUsingDSP(int wX, int wY);

		bool hasFaithfullProducts() { return false; }




		/**
		 * Returns the average required number of LUTs for the compression
		 * of one bit. The value is experimentally estimated and is
		 * dependent on the respective compressors available on the
		 * specific architecture.
		 * @param none
		 * @return the average LUTs to compress one bit
		 */
		virtual double getBitHeapCompressionCostperBit();
		


	protected:
		/* Attributes that belong to the FPGA and are therefore static */
		std::string id_;

		std::string vendor_;
		double maxFrequencyMHz_ ;   /**< The maximum practical frequency attainable on this target. An indicator of relative performance of FPGAs. 400 is for Virtex4 */
		int    lutInputs_;          /**< The number of inputs for the LUTs */
		// DSP related
		bool   hasHardMultipliers_; /**< If true, this target offers hardware multipliers */
		std::vector<std::pair<int,int>> possibleDSPConfig_; /**< configs in the signed case, e.g. virtexII will be only (18,18), virtex6 (25,18), stratixIV will be (9,9), ... (36,36). Largest (preferred) last */
		std::vector<bool> whichDSPCongfigCanBeUnsigned_; /** which DSP configs can be used full size as unsigned*/
		// TODO probably add a method to help chose among configs 
		bool   hasFastLogicTernaryAdders_; /**< If true, this target offers support for ternary addition at the cost of binary addition */
		int    multXInputs_;        /**< The size for the X dimension of the hardware multipliers (the largest, if they are not equal) */
		int    multYInputs_;        /**< The size for the Y dimension of the hardware multipliers  (the smallest, if they are not equal)*/
		int    registerLevelsInDSP_; /**< How many register levels inside a DSP TODO: not set yet in actual targets */
		// block memory related
		bool   hasMemoryBlock_;     /**< If true, this target offers hardware memory blocks */
		long   sizeOfBlock_;		    /**< The size of a primitive memory block */

		/* Attributes that belong to the application context and may be modified by the command line
		 */
		bool   pipeline_;           /**< True if the target is pipelined/ false otherwise */
		double frequency_;          /**< The desired frequency for the operator in Hz */
		bool   useClockEnable_;     /**< True if we want a clock enable signal */
		bool   nameSignalByCycle_;     /**< True if we want to rename signals by cycle number instead of delay */
		bool   useHardMultipliers_;        /**< True if we want to use DSPs, False if we want to generate logic-only architectures */
		float  unusedHardMultThreshold_;/**< Between 0 and 1. When we have a multiplier smaller than (or equal to) a DSP in both dimensions,
																		let r=(sub-multiplier area)/(DSP area); r is between 0 and 1
																		if r >= 1-unusedHardMultThreshold_   then FloPoCo will use a DSP for this block
																		So: 0 means: any sub-multiplier that does not fully fill a DSP goes to logic
																		1 means: any sub-multiplier, even very small ones, go to DSP*/
		bool   plainVHDL_;     /**< True if we want the VHDL code to be concise and readable, with + and * instead of optimized FloPoCo operators. */
		bool   registerLargeTables_;     /**< if true, a register is forced on the output of a Table objects that is larger than the blockRAM size, otherwise BlockRAM will not be used. Defaults to true, but sometimes you want to force a large table into LUTs. */
		bool   tableCompression_;     /**< if true, Hsiao table compression will be used. Should default to true, the flag is there for experiments measuring how useful it is */
		bool   generateFigures_;  /**< If true, some operators will generate figures which will clutter your directory  */
        bool   useTargetOptimizations_; /**< If true, target specific optimizations using primitives are performed. Vendor specific libraries are necessary for simulation. */

		std::string compression_; /**< Defines the BitHeap compression method*/
		std::string tiling_; /**< Defines the multiplier tiling method*/
		std::string ilpSolverName_; /*** Defines the ILP solver for operators optimized by ILP. It has to match a solver name known by the ScaLP library */
		int ilpTimeout_; /*** Defines the timeout in seconds for the ILP solver for operators optimized by ILP.*/
	};

}
#endif
