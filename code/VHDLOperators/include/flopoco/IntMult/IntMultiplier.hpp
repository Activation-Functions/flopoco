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


/* TODO: re-enable virtual multipliers as they were working in 4.1.2

		Two use cases among many others:
		   FixComplexMult computes a*b+c*d in a single bit heap
			 FixMultAdd computes a+b*c in a single bit heap.
		In both cases the result of the multiplication is provided as unevaluated bit in a bit heap.
		
		About rounding, guard bits, etc: this is the responsibility of the coarser operator (e.g. FixComplexMult).
		The virtual IntMultiplier just inputs an error bound and must make sure that the error of the multiplier is within this bound.
		It should also report the actual error achieved, sometimes it will be lower than the error bound.

		The virtual multiplier inputs two signal names, a BitHeap, and a position where to place the product in this BitHeap.
		This BitHeap object itself includes a pointer to an Operator which is the coarser op.
		For instance, it will generate code in bitheap->getOp()->vhdl.

		One thing that will make life simpler (and simpler than in 4.1.2) is to accept to construct a bit heap with empty columns.
		For instance for a faithful FixComplexMult, let us create a bit heap large enough for the exact products,
		even if we know they will be truncated.
		The two virtual instances of IntMultiplier will leave empty LSB columns and that's OK.
		
		A last technicality is that IntMultiplier is written with a LSB of 0, so there is some shifting to do.
		Here the simplest idea is probably to define this shift as exactProductLSBPosition, the position of the exact product LSB
		(which has position 0 in IntMultiplier).

		In summary, Workflows for the stand-alone and virtual multipliers
		* STAND-ALONE *: more or less what we have
			inputs wOut, computes g out of it, computes lsbWeightInBitHeap, creates a tiling, instantiates a bit heap, and throws bits in it.
			In case of truncation, adds a constant centering constant and a round bit

		* VIRTUAL *
			inputs signal names for X and Y, externalBitHeapPtr, and externalBitHeapPos.
			checks the BH is large enough, creates a tiling, throws bits in it with the corresponding VHDL in bitheap->getOp()->vhdl.
			adds the constant centering in case of truncation, but not the rounding bit: this is the responsibility of  bitheap->getOp()
		
		Corner cases that one has to understand and support:
			related to g, so stand-alone only: in general, g>0 when wOut<wX+wY.
			However, one situation where g=0 and wOut!=wX+wY is the tabulation of a correctly rounded small multiplier.


		As a consequence, attributes of IntMultiplier:
			- there should be no global attribute g, because it is used only in the stand-alone case.
			- there should be no global attribute wOut, because it is used only in the stand-alone case.
  			Actually there is a global attribute wOut, but to be used only by emulate() which is only used for the standalone case
			- fillBitHeap and the other methods should not refer to g or wOut

 */


namespace flopoco {
	class IntMultiplier : public Operator {

	public:

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
		IntMultiplier(Operator *parentOp, Target* target, int wX, int wY, int wOut=0, bool signedIO = false, int maxDSP=-1, bool superTiles=false, bool use2xk=false, bool useirregular=false, bool useLUT=true, bool useKaratsuba=false, bool useGenLUT=false, bool useBooth=false, int beamRange=0, bool optiTrunc=true, bool minStages=true, bool squarer=false, BitHeap* externalBitheapPtr=nullptr, int exactProductLSBPosition=0 );


		/** A virtual operator that adds to an existing BitHeap belonging to another Operator.
		 Also fixed-point oriented:
		   the formats of the inputs are read from the signals named xname and yname;
			 the multiplier should be faithful to lsbOut
		*/
		static void addToExistingBitHeap(BitHeap* externalBitheapPtr, string xname, string yname, int lsbOut, int exactProductLSBPosition=0 );



		
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

		/** TODO Deprecate once it has been properly split in two 
        * @brief Compute several parameters for a faithfully rounding truncated multiplier, supports the signed case
        * @details Compute guard-bits, keep-bits, the error re-centering constant and the error budget. The parameters are estimated by sucessivly removing partial products (as they would apperar in an and array, although the actual multiplier might use larger tiles) as long as the error bounds are met. The function considers the dynamic calculation of the error recentering constant to extend the permissible error to allow more truncated bits and hence a lower cost. In contrast to the algorithm published in the truncation paper mentioned below, this variant also considers that the singed case, where the partial products at the left and bottom edge |_ of the tiled area cause a positive when omitted (as they count negative in the result). The function has quadratic complexity.
        * @param wX width of the rectangle
        * @param wY height of the rectangle
        * @param wFull width of result of a non-truncated multiplier with the same input widths
        * @param wOut requested output width of the result vector of the truncated multiplier
        * @param g the number of bits below the output LSB that we need to keep in the summation
        * @param signedIO signedness of the multiplier
        * @param k number of bits to keep in in the column with weight w-g
        * @param errorBudget maximal permissible weight of the sum of the omitted partial products (as they would appear in an array multiplier)
        * @param constant to recenter the truncation error around 0 since it can otherwise only be negative, since there are only partial products left out. This allows a larger error, so more products can be omitted
        * @return none
        */
        static void computeTruncMultParamsMPZ(unsigned wX, unsigned wY, unsigned wFull, unsigned wOut, bool signedIO, unsigned &g, unsigned &k, mpz_class &errorBudget, mpz_class &constant);

		
		/** 
        * @brief Compute the guard bits, keep bits, and the error-centering constant for a faithfully rounding truncated multiplier
        * @details See 7.2.4.2 of Application Specific Arithmetic 2024.
				In addition to the algorithm there, this variant takes into account the negative sign of the partial products at the left and bottom edge |_ in signed multipliers.
				This should never be useful in a multiplier computing just right (see 8.1.4), but the world conspires to prove my intuitions wrong.
				This function is assuming an integer multiplier (lsbX=lsbY=0), so the error budget and the constant are integers
				This function has quadratic complexity.
        * @param wX size in bits of input X 
        * @param wY size in bits of input Y
        * @param errorBudget strict upper bound on the allowed error after truncation 
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
		Operator* realiseTile(
				TilingStrategy::mult_tile_t & tile,
				size_t idx,
				string output_name
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
         * @brief Define and calculate the size of the output signals of each multiplier tile in the solution to the BitHeap
         * @param bh BitHeap instance, where the partial results form the multiplier tiles should compressed
         * @param solution list of the placed tiles with their parametrization and anchor point
         * @param bitheapLSBWeight weight (2^bitheapLSBWeight) of the LSB that should be compressed on BH. It is supposed to be 0 for regular multipliers, but can be higher for truncated multipliers.
         * @return none
         */
		void fillBitheap(BitHeap* bh, list<TilingStrategy::mult_tile_t> &solution, unsigned int bitheapLSBWeight);

		// TODO remove all the following
        static unsigned additionalError_n(unsigned int wX, unsigned int wY, unsigned int col, unsigned int t, unsigned int wFull, bool signedIO);

        static unsigned additionalError_p(unsigned int wX, unsigned int wY, unsigned int col, unsigned int t, unsigned int wFull, bool signedIO);

        static mpz_class calcErcConst(mpz_class &errorBudget, mpz_class &wlext, mpz_class &deltap, mpz_class constant=1);

        void createFigures(TilingStrategy *tilingStrategy) const;
    };

}
#endif
