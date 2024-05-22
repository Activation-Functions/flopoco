#ifndef IntMultiplierS_HPP
#define IntMultiplierS_HPP
#include <sstream>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>

#include "flopoco/BitHeap/BitHeap.hpp"
#include "flopoco/IntMult/BaseMultiplierCategory.hpp"
#include "flopoco/IntMult/MultiplierBlock.hpp"
#include "flopoco/IntMult/MultiplierTileCollection.hpp"
#include "flopoco/IntMult/TilingStrategy.hpp"
#include "flopoco/InterfacedOperator.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/Tables/Table.hpp"
#include "flopoco/utils.hpp"

/* TODO get rid of most of
	  int maxDSP=-1, bool superTiles=false, bool use2xk=false, bool useirregular=false, bool useLUT=true,  bool useKaratsuba=false, bool useGenLUT=false, bool useBooth=false, int beamRange=0, bool optiTrunc=true, bool minStages=true */


/* Work in progress: re-enable virtual multipliers as they were working in 4.1.2

	 Some use cases among many others that the current IntMultiplier cannot address :
		   FixComplexMult computes a*b+c*d in a single bit heap
			 FixMultAdd computes a+b*c in a single bit heap.
			 FixSumOfProducts compute the faithful sum of N products.
	 In all these cases, we want a BitHeap shared by several IntMultipliers.
	 The simplest solution is to have a IntMultiplier method that throws VHDL into an existing Operator and the bits in an existing BitHeap.
	 Since a BitHeap is associated to an Operator, we just need to pass the BitHeap and we can recover the Operator from it. 

	 In summary, we should have two ways to instantiate an IntMultiplier:
		* STAND-ALONE *: more or less what we have
			inputs wOut, computes g out of it, computes lsbWeightInBitHeap, creates a tiling, instantiates a bit heap, and throws bits in it.
			In case of truncation, adds a constant centering constant and a round bit

		* VIRTUAL *
		In such cases the result of the multiplication is provided as unevaluated bits in a bit heap.
		It is a static IntMultiplier method that inputs
		  signal names for X and Y, absoluteError, externalBitHeapPtr, and externalBitHeapPos.
		It checks the BH is large enough, creates a tiling, throws bits in it (with the corresponding VHDL going to bitheap->getOp()->vhdl)
			then adds the centering  constant in case of truncation, but not the rounding bit: this is the responsibility of the parent bitheap->getOp()
		
		The coarser operator (e.g. FixComplexMult or FixSumOfProducts) implements an error analysis, 
	   and defines an error budget for each IntMultiplier.
		The virtual IntMultiplier just inputs this errorBudget and must make sure that the error of the multiplier is within this bound,
		  using the least possible bitheap columns.
		It should also report its LSB and the actual error achieved, sometimes it will be lower than the error bound.

		Each tiling is responsible for computing the recentering constant in case of a truncation.


		An example of problem with previous code:
		wOut is an attribute of the IntMultiplier class. This is OK: we need it for emulate() etc.
    BUT it was used all over the place in the tilings etc. Sometimes messed with guardBits, etc.
		This is not OK as virtual multipliers input an anchor (the position of the LSB of the exact product) and an accuracy,
		  and somehow compute their internal truncation out of this. 
		So wOut should be removed from the tiling inputs. Same for other parameters (guardBits...).
		All algorithms should become error-centered (some already are)
		
		One thing that will make life simpler (and simpler than in 4.1.2) is to accept to construct a bit heap with empty columns.
		For instance for a faithful FixComplexMult, let us create a bit heap large enough for the exact products,
		even if we know they will be truncated.
		The two virtual instances of IntMultiplier will leave empty LSB columns and that's OK. I think.
		
		IntMultiplier is written with a LSB of 0, which is generally a good idea. But to use it in a fixed-point context there is some shifting to do.
		Here the simplest idea is probably to define this shift as exactProductLSBPosition, the position of the ulp of the exact product.
		(position 0 in an integer multiplier). This will be easy

		An intermediate objective is to refactor IntMultiplier so that the stand-alone operator invokes the virtual one (to avoid code duplication)
		Current state of the refactoring:
		  most of the "staticization" is OK
			the refactored version works for exact (full) multipliers (and the default beam search)
			It still doesn't work for truncated ones.
			
		One bad design choice is to think/write the code in terms of wOut and wOut+g (because the msb is tricky, see prodsize())
		Better use LSBs internally
		
		Corner cases that one has to understand and support:
			related to g, so stand-alone only: in general, g>0 when wOut<wX+wY.
			However, one situation where g=0 and wOut!=wX+wY is the tabulation of a correctly rounded small multiplier.


		About attributes of IntMultiplier:
			- there should be no global attribute g, because it is used only in the stand-alone case.
			- there should be no global attribute wOut, because it is used only in the stand-alone case.
  			Actually there is a global attribute wOut, but to be used only by emulate() which is only used for the standalone case
			- fillBitHeap and the other methods should not refer to the attribg or wOut

		About tiling:
			- A tilingStrategy should only input wX, wY, and an errorBudget (expressed in ulps of the exact product as an mpz)
			  (plus all sorts of optional optimization flags if you want)
			- it should report
			     the error actually achieved,
					 an LSB to which all the tiles should be truncated (for the fillBitHeap method of IntMultiplier) 
			

			
 */


namespace flopoco {
	class IntMultiplier : public Operator {

	public:


		/** A virtual operator that adds to an existing BitHeap belonging to another Operator.
		 Also fixed-point oriented:
		   the formats of the inputs are read from the signals named xname and yname;
			 the error is given as an mpz_class and is an error in ulps of the exact product
			 TODO Frankly I don't know what is a tilingStrategy and why it should be returned... 
		*/
		static TilingStrategy* addToExistingBitHeap(BitHeap* externalBitheapPtr, string xname, string yname, mpz_class errorBudget, int exactProductLSBPosition=0 );


		
		/**
		 * The IntMultiplier constructor
		 * @param[in] target           the target device
		 * @param[in] wX             X multiplier size (including sign bit if any)
		 * @param[in] wY             Y multiplier size (including sign bit if any)
		 * @param[in] wOut           wOut size for a truncated multiplier (0 means full multiplier)
		 * @param[in] signedIO       false=unsigned, true=signed
		 * @param[in] texOutput      true=generate a tex file with the found tiling solution
		 * @param[in] externalBitheapPtr     if true, throw the multiplier bits in the provided bit heap
		 * @param[in] externalBitheapPos if externalBitheapPtr true, this is the position of the LSB of the exact product
		 **/
		IntMultiplier(Operator *parentOp, Target* target, int wX, int wY, int wOut=0, bool signedIO = false, int maxDSP=-1, bool superTiles=false, bool use2xk=false, bool useirregular=false, bool useLUT=true, bool useKaratsuba=false, bool useGenLUT=false, bool useBooth=false, int beamRange=0, bool optiTrunc=true, bool minStages=true, bool squarer=false);




		
		/**
		 * The emulate function.
		 * @param[in] tc               a test-case
		 */
		void emulate ( TestCase* tc );

		void buildStandardTestCases(TestCaseList* tcl);

		/** Factory method that parses arguments and calls the constructor */
		static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface& ui);

		/**
		 * @brief Compute the size required to store the untruncated product of inputs of a given width
		 * @param wX size of the first input
		 * @param wY size of the second input
		 * @return the number of bits needed to store a product of I<wX> * I<WY>
		 */
		static unsigned int prodsize(unsigned int wX, unsigned int wY, bool signedX, bool signedY);

		static TestList unitTest(int index);

		BitHeap* getBitHeap(void) {return bitHeap;}

		/**
         * @brief Checks if a tiling for a truncated multiplier meets the error budget as required for faithfulness
         * @param solution list of the placed tiles with their parametrization and anchor point
         * @param guardBits the number of bits below the output LSB that we need to keep in the summation
         * @param errorBudget maximal permissible weight of the sum of the omitted partial products (as they would appear in an array multiplier)
         * @param constant to recenter the truncation error around 0 since it can otherwise only be negative, since there are only partial products left out. This allows a larger error, so more products can be omitted
         * @param wX width (input word size) in x-direction of multiplier
         * @param wY height (input word size) in y-direction of multiplier
         * @param signedIO signedness of the multiplier
         * @return truncation error as MPZ value
         */
		static mpz_class checkTruncationError(list<TilingStrategy::mult_tile_t> &solution, unsigned int guardBits, const mpz_class& errorBudget, const mpz_class& constant, int wX, int wY, bool signedIO) ;

		/**
         * @brief calculate the width of the diagonal of a rectangle, that is equivalent to the number of partial product bits in the hypothetical resulting bitheap of a Bough-Wooley-Multiplier
         * @param wX width of the rectangle
         * @param wY height of the rectangle
         * @param col sum of the coordinates (x+y) that describe the diagonal
         * @param wFull full product width of the rectangular multiplier
         * @return number of partial products in the diagonal
         */
		static unsigned int widthOfDiagonalOfRect(unsigned int wX, unsigned int wY, unsigned int col, unsigned wFull, bool signedIO=false);
		
		/** 
        * @brief Compute the guard bits, keep bits, and the error-centering constant for truncated multiplier respecting a given error budget
        * @details See 7.2.4.2 of Application Specific Arithmetic 2024.
				TODO: this variant does not take into account the negative sign of the partial products at the left and bottom edge |_ in signed multipliers.
				This should never be useful in a multiplier computing just right (see 8.1.4), but the world conspires to prove my intuitions wrong.
        * @param wX size in bits of input X 
        * @param wY size in bits of input Y
        * @param errorBudget strict upper bound on the allowed error after truncation: |error| should be < errorBudget 
        * @param actualLSB the rightmost position where we need to keep bits
        * @param k number of bits to keep in the rightmost position
        * @param constant to recenter the truncation error around 0 since it can otherwise only be negative, since there are only partial products left out. This allows a larger error, so more products can be omitted. 
        * @return none
        */
		static void computeTruncMultParams(unsigned wX, unsigned wY, mpz_class errorBudget, unsigned &actualLSB, unsigned &k, mpz_class &constant);

		/**
         * @brief Calculate the LSB of the BitHeap required to maintain faithfulness, so that unnecessary LSBs to meet the error budget of multiplier tiles can be omitted from compression
         * @param solution list of the placed tiles with their parametrization and anchor point
         * @param guardBits the number of bits below the output LSB that we need to keep in the summation
         * @param errorBudget maximal permissible weight of the sum of the omitted partial products (as they would appear in an array multiplier)
         * @param constant to recenter the truncation error around 0 since it can otherwise only be negative, since there are only partial products left out. This allows a larger error, so more products can be omitted
         * @param actualTruncError the truncation error as previously determined by counting the untiled positions
         * @param wX width of the rectangle
         * @param wY height of the rectangle
         * @return LSB of the bitHeap
         */
		static int calcBitHeapLSB(list<TilingStrategy::mult_tile_t> &solution, unsigned guardBits, const mpz_class& errorBudget, const mpz_class& constant, const mpz_class& actualTruncError, int wX, int wY);

		static unsigned int negValsInDiagonalOfRect(unsigned wX, unsigned wY, unsigned col, unsigned wFull, bool signedIO);

	protected:

		unsigned int wX;                         /**< the width for X after possible swap such that wX>wY */
		unsigned int wY;                         /**< the width for Y after possible swap such that wX>wY */
		unsigned int wFullP;                     /**< size of the full product: wX+wY  */
		unsigned int wOut;                       /**< size of the output, to be used only in the standalone constructor and emulate.  */
		bool signedIO;                   /**< true if the IOs are two's complement */
		bool negate;                    /**< if true this multiplier computes -xy */
		float dspOccupationThreshold;   /**< threshold of relative occupation ratio of a DSP multiplier to be used or not */
		int maxDSP;            /**< limit the number of DSP-Blocks used in multiplier */
		BitHeap *bitHeap;
		bool squarer;          /**< generate squarer */

	private:
//		Operator* parentOp;  			/**< For a virtual multiplier, adding bits to some external BitHeap,
		/**
		 * Realise a tile by instantiating a multiplier, selecting the inputs and connecting the output to the bitheap
		 *
		 * @param tile: the tile to instentiate
		 * @param idx: the tile identifier for unique name
		 * @param output_name: the name to which the output of this tile should be mapped
		 */
		static Operator* realiseTile(OperatorPtr op,
																 TilingStrategy::mult_tile_t & tile,
																 size_t idx,
																 string output_name,
																 int wX, int wY,
																 bool squarer
																 );

		/** returns the amount of consecutive bits, which are not constantly zero
		 * @param bm:                          current BaseMultiplier
		 * @param xPos, yPos:                  position of lower right corner of the BaseMultiplier
		 * @param totalOffset:                 see placeSingleMultiplier()
		 * */
		unsigned int getOutputLengthNonZeros(
				BaseMultiplierParametrization const & parameter,
				unsigned int xPos,
				unsigned int yPos,
				unsigned int totalOffset
			);

		unsigned int getLSBZeros(
				BaseMultiplierParametrization const & parameter,
				unsigned int xPos,
				unsigned int yPos,
				unsigned int totalOffset,
				int mode
			);

		/**
		 * @brief Compute the number of bits below the output msb that we need to keep in the summation
		 * @param wX first input width
		 * @param wY second input width
		 * @param wOut number of bits kept in the output
		 * @return the the number of bits below the output msb that we need to keep in the summation to ensure faithful rounding
		 */
		unsigned int computeGuardBits(unsigned int wX, unsigned int wY, unsigned int wOut);

		/**
		 * add a unique identifier for the multiplier, and possibly for the block inside the multiplier
		 */
		string addUID(string name, int blockUID=-1);

		int multiplierUid;

        /**
         * @brief From a tiling solution, generates VHDL that implements these tiles and transfers their result to a BitHeap
         * @param bh BitHeap instance, where the partial results form the multiplier tiles should be sent
         * @param squarer boolean, true if this multiplier tiling is actually a squarer.
         * @return none
         */
		static void fillBitheap(BitHeap* bh, TilingStrategy *tiling, bool squarer);

		static void createFigures(TilingStrategy *tiling);
 

	};

}
#endif
