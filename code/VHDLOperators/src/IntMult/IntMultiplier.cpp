/*
  An integer multiplier for FloPoCo

  Authors:  Martin Kumm with parts of previous IntMultiplier of Bogdan Pasca, F de Dinechin, Matei Istoan, Kinga Illyes and Bogdan Popa

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2017.
  All rights reserved.
*/


// This is the switch to move to the version that doesn't work yet...
#define USE_REFACTORED 1



#include <cassert>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>

#include <gmp.h>
#include <gmpxx.h>
#include <mpfr.h>

#include "flopoco/IntAddSubCmp/IntAdder.hpp"
#include "flopoco/IntMult/BaseMultiplierCollection.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/IntMult/TilingAndCompressionOptILP.hpp"
#include "flopoco/IntMult/TilingStrategyBasicTiling.hpp"
#include "flopoco/IntMult/TilingStrategyBeamSearch.hpp"
#include "flopoco/IntMult/TilingStrategyGreedy.hpp"
#include "flopoco/IntMult/TilingStrategyOptimalILP.hpp"
#include "flopoco/IntMult/TilingStrategyXGreedy.hpp"
#include "flopoco/IntMult/TilingStrategyCSV.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/UserInterface.hpp"
#include "flopoco/utils.hpp"

using namespace std;

namespace flopoco {



	

	
	IntMultiplier::IntMultiplier (Operator *parentOp, Target* target_, int wX_, int wY_, int wOut_, bool signedIO_, int maxDSP, bool superTiles, bool use2xk, bool useirregular, bool useLUT, bool useKaratsuba, bool useGenLUT, bool useBooth, int beamRange, bool optiTrunc, bool minStages, bool squarer):
		Operator ( parentOp, target_ ),wX(wX_), wY(wY_), wOut(wOut_),signedIO(signedIO_),  squarer(squarer)
	{
		srcFileName = "IntMultiplier";
		setCopyrightString("Martin Kumm, Florent de Dinechin, Kinga Illyes, Bogdan Popa, Bogdan Pasca, 2012-");
		
		dspOccupationThreshold = target_->unusedHardMultThreshold();
		bool useDSP = target_->useHardMultipliers();
		
		
		wFullP = prodsize(wX, wY, signedIO_, signedIO_);
		if (wOut == 0 || wFullP < wOut) {
			wOut = wFullP;
		}
		ostringstream name, trunc_info;
		name << "IntMultiplier_"<< wX << "x" << wY<< "_"<< wOut;
		setNameWithFreqAndUID(name.str());
		
		// the addition operators need the ieee_std_signed/unsigned libraries
		useNumericStd();
		
		multiplierUid = parentOp->getNewUId();

		string xname="X";
		string yname="Y";

		// Set up the IO signals
		addInput(xname, wX, true);
		addInput(yname, wY, true);
		addOutput("R", wOut, true);

		// The larger of the two inputs
		vhdl << tab << declare(addUID("XX"), wX, true) << " <= " << xname << " ;" << endl;
		vhdl << tab << declare(addUID("YY"), wY, true) << " <= " << yname << " ;" << endl;

		if(target_->plainVHDL()) {
			vhdl << tab << declareFixPoint("XX",signedIO,-1, -wX) << " <= " << (signedIO?"signed":"unsigned") << "(" << xname <<");" << endl;
			vhdl << tab << declareFixPoint("YY",signedIO,-1, -wY) << " <= " << (signedIO?"signed":"unsigned") << "(" << yname <<");" << endl;
			vhdl << tab << declareFixPoint("RR",signedIO,-1, -wX-wY) << " <= XX*YY;" << endl;
			vhdl << tab << "R <= std_logic_vector(RR" << range(wX+wY-1, wX+wY-wOut) << ");" << endl;

			return;
		}
		
		BitHeap* bitHeap = new BitHeap(this, wFullP);
		
		// Current status is: exact multipliers work.
		// TODO compute the error correction constant and understand what truncated tiling does.

		// error budget is one half-ulp, and there is also one half-ulp for final rounding
		int lsbOut = wFullP - wOut;
		mpz_class errorBudget = 0;
		if(wOut<wFullP) {
			errorBudget = mpz_class(1) << (lsbOut-1);
		}
		REPORT(LogLevel::DEBUG,  " errorBudget=" << errorBudget);

		// calling the virtual constructor that does all the work
		auto tilingStrategy = addToExistingBitHeap(bitHeap,  xname,  yname, errorBudget, 0);
		
		if (dynamic_cast<CompressionStrategy*>(tilingStrategy)) { // for the combined tiling+compression, currently disabled  
			REPORT(LogLevel::DEBUG,  "Class is derived from CompressionStrategy, passing result for compressor tree.");
			bitHeap->startCompression(dynamic_cast<CompressionStrategy*>(tilingStrategy));
		}
		else { // normal process, tiling then compression
			// Add the rounding bit for a faithfully rounded truncated multiplier  
			if (wOut<wFullP) {
				bitHeap->addConstantOneBit(lsbOut-1);
			}
			bitHeap->startCompression();
		}
		vhdl << tab << "R" << " <= " << bitHeap->getSumName() << range(wFullP-1, wFullP-wOut) << ";" << endl;
		delete bitHeap;	
		delete tilingStrategy;

	}


	// the virtual multiplier method
	TilingStrategy*	IntMultiplier::addToExistingBitHeap(BitHeap* bh, string xname, string yname, mpz_class errorBudget, int exactProductLSBPosition)
	{
		OperatorPtr op=bh->getOp();
		Signal* x = op -> getSignalByName(xname);
		Signal* y = op -> getSignalByName(yname);
		// let's get over with the fixed-point stuff
		int msbX = x->MSB();
		int msbY = y->MSB();
		int lsbX = x->LSB();
		int lsbY = y->LSB();
		int wX = x->width();
		int wY = y->width();
		int signedX = x->isSigned();
		int signedY = y->isSigned();
		if (signedX!=signedY) {
			ostringstream e;
			e << "Product of two signals " << xname << " and " << yname << " with different signednesses is not supported in IntMultiplier::addToExistingBitHeap";
			throw(e.str());
		}
		
		int lsbPfull = lsbX+lsbY; // This is the anchor for exactProductLSBPosition

		// from now on we switch to the integer-multiplier point of view of the Fulda code
		int wFullP = prodsize(wX, wY, signedX, signedY);

		// Generate a tiling

		// Useless noise that I have to live with because Fulda didn't clean it up
		int maxDSP=-1;
		bool superTiles=false;
		bool use2xk=false;
		bool useirregular=false;
		bool useLUT=true;
		bool useKaratsuba=false;
		bool useGenLUT=false;
		bool useBooth=false;
		int beamRange=0;
		bool optiTrunc=true;
		bool minStages=true;
		bool squarer=false;
		float dspOccupationThreshold = op->getTarget()->unusedHardMultThreshold();
		bool useDSP = op->getTarget()->useHardMultipliers();

		REPORT(LogLevel::DETAIL, "Creating BaseMultiplierCollection");
		BaseMultiplierCollection baseMultiplierCollection(op->getTarget());
		//		baseMultiplierCollection.print();

		MultiplierTileCollection multiplierTileCollection(op->getTarget(), &baseMultiplierCollection, wX, wY, superTiles, use2xk, useirregular, useLUT, useDSP, useKaratsuba, useGenLUT, useBooth, squarer);

		string tilingMethod = op->getTarget()->getTilingMethod();

		REPORT(LogLevel::DETAIL, "Creating TilingStrategy using tiling method " << tilingMethod);

		// // TODO HERE I have to invent a wOut and a guardBits for this to compile,
		// // but I shouldn't have to
		// int wOut, guardBits;
		// if(errorBudget==0)  {
		// 	wOut=wFullP;
		// 	guardBits=0;
		// }
		// else { // what should I do here?
		// 	int lsbOut=0;
		// 	int wOut=wFullP;
		// 	mpz_class error=1;
		// 	while(error<errorBudget) {
		// 		lsbOut++;
		// 		wOut--;
		// 		error = error<<1;
		// 	} // This was an intlog2
		// 	int guardBits=0;
		// 	// let's assume that the inconsistent interface could be compressed into  wOut+guardBits
		// }
		TilingStrategy* tilingStrategy;
		// Message to Andreas:
		// Florent commented out all these methods for you to revive them someday once I have converged on the proper interface.
		// (still TODO)
		
		 if(tilingMethod.compare("heuristicBasicTiling") == 0) {
			tilingStrategy = new TilingStrategyBasicTiling(
					wX,
					wY,
					signedX,
					errorBudget, 
					&baseMultiplierCollection,
					baseMultiplierCollection.getPreferedMultiplier(),
					dspOccupationThreshold,
					((maxDSP<0)?(unsigned)INT_MAX:(unsigned)((useDSP)?maxDSP:0))	);
		 }
		// else if(tilingMethod.compare("heuristicGreedyTiling") == 0) { 
		// 	tilingStrategy = new TilingStrategyGreedy(
		// 			wX,
		// 			wY,
		// 			wOut,
		// 			signedX,
		// 			&baseMultiplierCollection,
		// 			baseMultiplierCollection.getPreferedMultiplier(),
		// 			dspOccupationThreshold,
		// 			maxDSP,
		// 			useirregular,
		// 			use2xk,
		// 			superTiles,
		// 			useKaratsuba,
		// 			multiplierTileCollection,
		// 			guardBits,
		// 			lastColumnKeepBits );
		// }
		// else if(tilingMethod.compare("heuristicXGreedyTiling") == 0) {
		// 	tilingStrategy = new TilingStrategyXGreedy(
		// 			wX,
		// 			wY,
		// 			wOut,
		// 			signedX,
		// 			&baseMultiplierCollection,
		// 			baseMultiplierCollection.getPreferedMultiplier(),
		// 			dspOccupationThreshold,
		// 			maxDSP,
		// 			useirregular,
		// 			use2xk,
		// 			superTiles,
		// 			useKaratsuba,
		// 			multiplierTileCollection,
		// 			guardBits,
		// 			lastColumnKeepBits );
		// }
	  // else
		// if(tilingMethod.compare("heuristicBeamSearchTiling") == 0) {
		// 	tilingStrategy = new TilingStrategyBeamSearch(
		// 																								wX,
		// 																								wY,
		// 																								wOutPlusGuardBits, // was wOut
		// 																								signedX,
		// 																								&baseMultiplierCollection,
		// 																								baseMultiplierCollection.getPreferedMultiplier(),
		// 																								dspOccupationThreshold,
		// 																								maxDSP,
		// 																								useirregular,
		// 																								use2xk,
		// 																								superTiles,
		// 																								useKaratsuba,
		// 																								multiplierTileCollection,
		// 																								beamRange,
		// 																								0, // wasguardBits,
		// 																								lastColumnKeepBits	);
		// }
		// 	else if(tilingMethod.compare("optimal") == 0){
		// 	tilingStrategy = new TilingStrategyOptimalILP(
		// 			wX,
		// 			wY,
		// 			wOut,
		// 			signedX,
		// 			&baseMultiplierCollection,
		// 			baseMultiplierCollection.getPreferedMultiplier(),
		// 			dspOccupationThreshold,
		// 			maxDSP,
		// 			multiplierTileCollection,
		// 			guardBits,
		// 			lastColumnKeepBits,
		// 			errorBudget,
		// 			centerErrConstant,
		// 			optiTrunc,
		// 			squarer );
		// }
		// 	else if(tilingMethod.compare("optimalTilingAndCompression") == 0){
		// 	tilingStrategy = new TilingAndCompressionOptILP(
		// 			wX,
		// 			wY,
		// 			wOut,
		// 			signedX,
		// 			&baseMultiplierCollection,
		// 			baseMultiplierCollection.getPreferedMultiplier(),
		// 			dspOccupationThreshold,
		// 			maxDSP,
		// 			multiplierTileCollection,
		// 			bh,
		// 			guardBits,
		// 			lastColumnKeepBits,
		// 			errorBudget,
		// 			centerErrConstant,
		// 			optiTrunc,
		// 			minStages
		// 	);

		// }
		// else if(tilingMethod.compare("csv") == 0) {
		// 	tilingStrategy = new TilingStrategyCSV(
		// 																				 wX,
		// 																				 wY,
		// 																				 wOutPlusGuardBits, //was wOut
		// 																				 signedX,
		// 																				 &baseMultiplierCollection,
		// 																				 baseMultiplierCollection.getPreferedMultiplier(),
		// 																				 dspOccupationThreshold,
		// 																				 maxDSP,
		// 																				 useirregular,
		// 																				 use2xk,
		// 																				 superTiles,
		// 																				 useKaratsuba,
		// 																				 multiplierTileCollection,
		// 																				 0,// was guardBits,
		// 																				 lastColumnKeepBits );
		// }
		else {
			ostringstream e;
			e << "Tiling strategy " << tilingMethod << " unknown";
			throw(e.str());
		}
			
		REPORT(LogLevel::DETAIL, "Solving tiling problem");
		tilingStrategy->solve();




		// Trimming signs? Not sure what this does.
		if(signedX) {
			for(auto & tile : tilingStrategy->getSolution()) {
				//resize DSPs to be aligned with left and bottom border of the tiled area to allow the correct handling of the sign
				tile.first = tile.first.shrinkFitDSP(tile.second.first,tile.second.second, wX, wY);
				//Set signedness of individual tiles according to their position
				tile.first = tile.first.setSignStatus(tile.second.first,tile.second.second, wX, wY, signedX);
			}
		}

		// Various outputs of the solution
		tilingStrategy->printSolution();
		auto solLen = tilingStrategy->getSolution().size();
		REPORT(LogLevel::VERBOSE, "Found solution has " << solLen << " tiles");
		if (op->getTarget()->generateFigures())
			createFigures(tilingStrategy);
		
		op -> schedule(); // This schedules up to the inputs of the multiplier

		// Now we want to transfer this tiling to the bit heap.
		// Sanity check: is the bit heap large enough? (another option would be to silently enlarge it)
		unsigned actualLSB = tilingStrategy->getActualLSB(); // the position of the last column on which we have bits
		//		unsigned lastColumnKeepBits = tilingStrategy->getLastColumnKeepBits(); // the number of bits to keep on the last column: commented out because mostly useless
		mpz_class centerErrConstant= tilingStrategy->getErrorCorrectionConstant(); // recentering constant to add to the bit heap
		REPORT(LogLevel::MESSAGE, "errorBudget=" << errorBudget << " actualLSB=" << actualLSB << "  centerErrConstant=" << centerErrConstant);
		
		if(bh->lsb > lsbPfull + actualLSB) {
			ostringstream e;
			e << "In IntMultiplier::addToExistingBitHeap, we need a bit heap up to LSB=" << lsbPfull + actualLSB << " but provided bit heap only extends to LSB=" << bh->lsb;
			throw(e.str());
		}
		
		if(bh->msb < lsbPfull+wFullP-1) {
			ostringstream e;
			e << "In IntMultiplier::addToExistingBitHeap, we need a bit heap up to MSB=" << lsbPfull+wFullP-1 << " but provided bit heap only extends to MSB=" << bh->msb;
			throw(e.str());
		}
		unsigned wOutPlusGuardBits = wFullP - actualLSB;
		



		// Transfer the tiling, including the correction constant, to the bit heap
		cerr << "XXXXXXXXXXXXXXXXXXXX " << actualLSB << "  " <<  exactProductLSBPosition << endl;
		fillBitheap(bh, tilingStrategy, squarer);

		bh -> printBitHeapStatus();
		op -> schedule(); // This schedule up to the compressor tree
		return tilingStrategy; // to transfer useful parameters such as actualLSB
	}





	
	/** Simple auxiliary function that computes the height of a column in a standard (non-Booth) multiplier */
	int computePlainMultColHeight(int wX, int wY, int col) {
		/*	The bit array for a 2x6 multiplier, to illustrate the logic:
		 *  xxxxxx
		 * xxxxxx      */
		int colHeight;
		if (col<std::min(wX,wY)) {
			colHeight=col+1;
		} else if (col < std::max(wX,wY)) {
			colHeight=std::min(wX,wY);
		}
		else {
			cerr << "IntMultiplier::computeTruncMultParams() invoked for wX=" << wX << ", wY=" << wY
					 << " and we are now truncating col " << col
					 << " which doesn't make much sense: you probably had better truncate the inputs." << endl
					 << " Attempting to proceed nevertheless but it could end in tears." << endl;
			if (col<wX+wY-1) {
				colHeight = wX+wY-1-col;
			}
			else { // Here we would enter an infinite loop.
				colHeight=0; 
				ostringstream error;
				error << "IntMultiplier::computeTruncMultParams() was invoked for wX=" << wX << ", wY=" << wY <<
					" with an error budget that means: remove this multiplier." << endl <<
					"Somebody is doing something that doesn't make sense, I'd rather crash now than later." <<endl;
				throw(error.str());
			} 
		}
		return colHeight;
	}

	// The version of Florent with no external function.
	// It only works for a plain multiplier, not for a squarer nor a Booth one
	// To cover this, this function should become a static method of BitHeap that just inputs an array of column heights and an error budget
	// The computation of the error correction constant is not that of The Book, because we input errorBudget, but the result is the same when called for a faithful mult
	void IntMultiplier::computeTruncMultParams(unsigned wX, unsigned wY, mpz_class errorBudget, unsigned &actualLSB, unsigned &keepBits, mpz_class &errorCenteringConstant){
		actualLSB=0;
		errorCenteringConstant=0;
		keepBits=1; // for the case errorBudget==0 
		if(errorBudget==0) {
			return; 
		}
		// Otherwise we start removing bits from the right.
		mpz_class wcSumOfTruncatedBits=0; // worst-case sum of all the truncated bits (assuming they are all 1)
		unsigned colHeight = 1; // height of the rightmost column of an untruncated multiplier
		mpz_class weightOfNextBitToBeRemoved = 1; // we start with position actualLSB=0, whose weight is 1
		errorCenteringConstant=errorBudget-1; // if we truncate nothing, this constant works
		// For readability I use a stupid quadratic algorithm instead of the linear one in the book
		bool keepGoing=true;
		while(keepGoing) {
			wcSumOfTruncatedBits += weightOfNextBitToBeRemoved;
			mpz_class tentativeMinError = -errorCenteringConstant; // when all truncated bits are 0; and by construction errorCenteringConstant < errorBudget
			mpz_class tentativeMaxError = wcSumOfTruncatedBits-errorCenteringConstant;
			if (tentativeMaxError<=errorBudget) { // one bit in col actualLSB can be removed 
				colHeight--;
				// cerr << "computeTruncMultParams: removing bit " <<k<< " in actualLSB " << actualLSB << ", current error=" << error << endl;
				if(0==colHeight) { // we have removed all the bits of this column
					actualLSB++; // move to the next column
					weightOfNextBitToBeRemoved = weightOfNextBitToBeRemoved<<1;
					colHeight=computePlainMultColHeight(wX, wY, actualLSB);
					// recompute the recentering constant: strictly smaller than errorBudget,
					// but truncated to have no bit lower than the current actualLSB 	
					errorCenteringConstant = ((errorBudget-1) >> actualLSB) << actualLSB;  
				}
			}
			else { //tentativeMaxError>errorBudget
				keepGoing=false;
			}
		}

		keepBits=colHeight;
		// restore to the last that worked
		wcSumOfTruncatedBits -= weightOfNextBitToBeRemoved;
			
		cerr << "computeTruncMultParams: Finally actualLSB=" << actualLSB << " keepBits=" << keepBits << " wcSumOfTruncatedBits=" << wcSumOfTruncatedBits   <<  " corrConstant=" << errorCenteringConstant << endl;			
		return;
	}



    unsigned int IntMultiplier::widthOfDiagonalOfRect(unsigned wX, unsigned wY, unsigned col, unsigned wFull, bool signedIO){
        if(signedIO){ wX--; wY--; wFull = prodsize(wX, wY, signedIO, signedIO);}
        if(0 <= col && (int)col <= min((int)wX, (int)wY)){
            return col;
        } else if(min((int)wX,(int)wY) < (int)col && (int)col <= max((int)wX, (int)wY)){
            return min((int)wX,(int)wY);
        } else {
            return wFull-col;
        }
    }

    unsigned int IntMultiplier::negValsInDiagonalOfRect(unsigned wX, unsigned wY, unsigned col, unsigned wFull, bool signedIO){
        return widthOfDiagonalOfRect(wX,wY,col ,wFull) - widthOfDiagonalOfRect(wX,wY,col ,wFull, signedIO);
    }

    /**
     * @brief Compute the size required to store the untruncated product of inputs of a given width
     * @param wX size of the first input
     * @param wY size of the second input
     * @return the number of bits needed to store a product of I<wX> * I<WY>
     */
    unsigned int IntMultiplier::prodsize(unsigned int wX, unsigned int wY, bool signedX, bool signedY)
    {
        if(wX == 0 || wY == 0)
            return 0;

        if (wX == 1 && wY == 1) {
            return 1;
        }

        if(wX == 1 && !signedX)
            return wY;

        if(wY == 1 && !signedY)
            return wX;

        return wX + wY;
    }

	/**
	 * @brief getZeroMSB if the tile goes out of the multiplier,
	 *		  compute the number of extra zeros that should be inputed to the submultiplier as MSB
	 * @param anchor lsb of the tile
	 * @param tileWidth width of the tile
	 * @param multiplierLimit msb of the overall multiplier
	 * @return
	 */
	inline unsigned int getZeroMSB(int anchor, unsigned int tileWidth, int multiplierLimit)
	{
		unsigned int result = 0;
		if (static_cast<int>(tileWidth) + anchor > multiplierLimit) {
			result = static_cast<unsigned int>(
						static_cast<int>(tileWidth) + anchor - multiplierLimit
						);
		}
		return result;
	}

	/**
	 * @brief getInputSignal Perform the selection of the useful bit and pad if necessary the input signal to fit a certain format
	 * @param msbZeros number of zeros to add left of the selection
	 * @param selectSize number of bits to get from the inputSignal
	 * @param offset where to start taking bits from the input signal
	 * @param lsbZeros number of zeros to append to the selection for right padding
	 * @param signalName input signal name from which to select the bits
	 * @return the string corresponding to the signal selection and padding
	 */
	inline string getInputSignal(unsigned int msbZeros, unsigned int selectSize, unsigned int offset, unsigned int lsbZeros, string const & signalName)
	{
		stringstream s;
		if (msbZeros > 0)
			s << zg(static_cast<int>(msbZeros)) << " & ";

		s << signalName << range(
				 static_cast<int>(selectSize - 1 + offset),
				 static_cast<int>(offset)
			);

		if (lsbZeros > 0)
			s << " & " << zg(static_cast<int>(lsbZeros));

		return s.str();
	}

	void IntMultiplier::fillBitheap(BitHeap* bitheap, TilingStrategy *tiling, bool squarer)
	{
		int wX=tiling->getwX();
		int wY=tiling->getwY();
		int bitheapLSBWeight=0; 
		auto solution = tiling->getSolution();
		mpz_class correctionConstant=tiling->getErrorCorrectionConstant();
		cerr << " ***** Entering Multiplier::fillBitheap with bitheapLSBWeight=" << bitheapLSBWeight << " and constant=" <<  correctionConstant << endl;
		// add the constant to start with
		bitheap->addConstant(correctionConstant, 0);
		// Now add the tiles
		size_t i = 0;
		stringstream oname, ofname;
		for(auto & tile : solution) {
			auto& parameters = tile.first;
			auto& anchor = tile.second;

			int xPos = anchor.first;
			int yPos = anchor.second;

			oname.str("");
			oname << "tile_" << i << "_output";
			realiseTile(bitheap->getOp(), tile, i, oname.str(), wX, wY, squarer);

			ofname.str("");
			ofname << "tile_" << i << "_filtered_output";

			if(parameters.getOutputWeights().size()){
			    //The multiplier tile has more then one output signal
				for(unsigned i = 0; i < parameters.getOutputWeights().size(); i++){
				    int LSBWeight = xPos + yPos + parameters.getOutputWeights()[i];
				    unsigned int outLSBWeight = (LSBWeight < 0)?0:static_cast<unsigned int>(LSBWeight);                         // calc result LSB weight corresponding to tile position
				    unsigned int truncated = (outLSBWeight < bitheapLSBWeight) ? bitheapLSBWeight - outLSBWeight : 0;           // calc result LSBs to be ignored
				    unsigned int bitHeapOffset = (outLSBWeight < bitheapLSBWeight) ? 0 : outLSBWeight - bitheapLSBWeight;       // calc bits between the tiles output LSB and the bitheaps LSB
				    unsigned int toSkip = ((LSBWeight < 0) ? static_cast<unsigned int>(-LSBWeight) : 0) + truncated;            // calc LSB bits to be ignored in the tiles output
				    int tokeep = parameters.getOutputSizes()[i] - toSkip;                                              // the tiles MSBs that are actually used
				    if(tokeep < 1) continue;    //the pariticular output of the tile does not contribute to the bitheap

				    unsigned int xInputLength = parameters.getTileXWordSize();
				    unsigned int yInputLength = parameters.getTileYWordSize();
				    bool xIsSigned = parameters.isSignedMultX();
				    bool yIsSigned = parameters.isSignedMultY();

				    if(tile.first.isSquarer()){xIsSigned=false; yIsSigned=false;}   //result of squarer tile is always unsigned

				    bool bothOne = (xInputLength == 1) && (yInputLength == 1);
				    bool signedCase = (bothOne and (xIsSigned != yIsSigned)) or ((not bothOne) and (xIsSigned or yIsSigned));

				    if(i && 31 < parameters.getMultType().size() && parameters.getMultType().substr(0, 31).compare("BaseMultiplierDSPKaratsuba_size") == 0){
				        		bitheap->getOp()->vhdl << bitheap->getOp()->declare(.0, ofname.str() + ((i)?to_string(i):""), parameters.getOutputSizes()[i]) << " <= " << "" << oname.str() + ((i)?to_string(i):"") + range(toSkip + tokeep - 1, toSkip) << ";" << endl;
				        bitheap->getOp()->getSignalByName(ofname.str() + ((i)?to_string(i):""))->setIsSigned();        //for Karatsuba the outputs have to be treated signed
				    } else {
				        bitheap->getOp()->vhdl << tab << bitheap->getOp()->declareFixPoint(.0, ofname.str() + ((i)?to_string(i):""), signedCase, tokeep-1, 0) << " <= " << ((signedCase) ? "" : "un") << "signed(" << oname.str() + ((i)?to_string(i):"") <<
				        range(toSkip + tokeep - 1, toSkip) << ");" << endl;
				    }

				    //squarers can have tile counted twice to exploit the symmetries, and hence might require a left shift by one bit or have tiles to be considered negative to compensate for overlap.
				    if(tile.first.getTilingWeight() == 1 || tile.first.getTilingWeight() == 2){
				        bitheap->addSignal(ofname.str() + ((i)?to_string(i):""), bitHeapOffset + ((tile.first.getTilingWeight() == 2)?1:0) );
				    } else {
				        bitheap->subtractSignal(ofname.str() + ((i)?to_string(i):""), bitHeapOffset + ((tile.first.getTilingWeight() == -2)?1:0) );
				    }

					cout << "output (" << i+1 << "/" << parameters.getOutputWeights().size() << "): " << ofname.str() + to_string(i) << " shift " << bitHeapOffset+parameters.getOutputWeights()[i] << endl;
				}
			} else {
                //The multiplier tile has only a single output signal
                int LSBWeight = xPos + yPos + parameters.getRelativeResultLSBWeight();
                unsigned int outLSBWeight = (LSBWeight < 0)?0:static_cast<unsigned int>(LSBWeight);                         // calc result LSB weight corresponding to tile position
                unsigned int truncated = (outLSBWeight < bitheapLSBWeight) ? bitheapLSBWeight - outLSBWeight : 0;           // calc result LSBs to be ignored
                unsigned int bitHeapOffset = (outLSBWeight < bitheapLSBWeight) ? 0 : outLSBWeight - bitheapLSBWeight;       // calc bits between the tiles output LSB and the bitheaps LSB
                unsigned int toSkip = ((LSBWeight < 0) ? static_cast<unsigned int>(-LSBWeight) : 0) + truncated;            // calc LSB bits to be ignored in the tiles output
                unsigned int tokeep = parameters.getRelativeResultMSBWeight() - toSkip - parameters.getRelativeResultLSBWeight()+1;                                     // the tiles MSBs that are actually used
                assert(tokeep > 0); //A tiling should not give a useless tile

				unsigned int xInputLength = parameters.getTileXWordSize();
				unsigned int yInputLength = parameters.getTileYWordSize();
				bool xIsSigned = parameters.isSignedMultX();
				bool yIsSigned = parameters.isSignedMultY();

				if(tile.first.isSquarer()){xIsSigned=false; yIsSigned=false;}   //result of squarer tile is always unsigned

				bool bothOne = (xInputLength == 1) && (yInputLength == 1);
				bool signedCase = (bothOne and (xIsSigned != yIsSigned)) or ((not bothOne) and (xIsSigned or yIsSigned));

				bitheap->getOp()->vhdl << tab << bitheap->getOp()->declareFixPoint(.0, ofname.str(), signedCase, tokeep-1, 0) << " <= " << ((signedCase) ? "" : "un") << "signed(" << oname.str() <<
						range(toSkip + tokeep - 1, toSkip) << ");" << endl;

				//squarers can have tile counted twice to exploit the symmetries, and hence might require a left shift by one bit or have tiles to be considered negative to compensate for overlap.
				if(tile.first.getTilingWeight() == 1 || tile.first.getTilingWeight() == 2){
				    bitheap->addSignal(ofname.str(), bitHeapOffset + ((tile.first.getTilingWeight() == 2)?1:0) );
				} else {
				    bitheap->subtractSignal(ofname.str(), bitHeapOffset + ((tile.first.getTilingWeight() == -2)?1:0) );
				}
			}
			i += 1;
		}
	}

	Operator* IntMultiplier::realiseTile(OperatorPtr op, TilingStrategy::mult_tile_t & tile, size_t idx, string output_name,
																			 int wX, int wY, bool squarer)
	{
		auto& parameters = tile.first;
		auto& anchor = tile.second;
		int xPos = anchor.first;
		int yPos = anchor.second;

		unsigned int xInputLength = parameters.getTileXWordSize();
		unsigned int yInputLength = parameters.getTileYWordSize();
		//unsigned int outputLength = parameters.getOutWordSize();

		unsigned int lsbZerosXIn = (xPos < 0) ? static_cast<unsigned int>(-xPos) : 0;
		unsigned int lsbZerosYIn = (yPos < 0) ? static_cast<unsigned int>(-yPos) : 0;

		unsigned int msbZerosXIN = getZeroMSB(xPos, xInputLength, wX);
		unsigned int msbZerosYIn = getZeroMSB(yPos, yInputLength, wY);

		unsigned int selectSizeX = xInputLength - (msbZerosXIN + lsbZerosXIn);
		unsigned int selectSizeY = yInputLength - (msbZerosYIn + lsbZerosYIn);

		unsigned int effectiveAnchorX = (xPos < 0) ? 0 : static_cast<unsigned int>(xPos);
		unsigned int effectiveAnchorY = (yPos < 0) ? 0 : static_cast<unsigned int>(yPos);

		auto tileXSig = getInputSignal(msbZerosXIN, selectSizeX, effectiveAnchorX, lsbZerosXIn, "X");
		auto tileYSig = getInputSignal(msbZerosYIn, selectSizeY, effectiveAnchorY, lsbZerosYIn, ((squarer && !tile.first.isSquarer()) ?"X":"Y"));

		stringstream nameX, nameY, nameOutput;
		nameX << "tile_" << idx << "_X";
		nameY << "tile_" << idx << "_Y";

		nameOutput << "tile_" << idx << "_Out";

		op->vhdl << tab << op->declare(0., nameX.str(), xInputLength) << " <= " << tileXSig << ";" << endl;
		if(!tile.first.isSquarer()) {
			op->vhdl << tab << op->declare(0., nameY.str(), yInputLength) << " <= " << tileYSig << ";" << endl;
		}

		string multIn1SigName = nameX.str();
		string multIn2SigName = nameY.str();

		if (parameters.isFlippedXY())
			std::swap(multIn1SigName, multIn2SigName);

		nameOutput.str("");
		nameOutput << "tile_" << idx << "_mult";

		op->inPortMap("X", multIn1SigName);
		if(!tile.first.isSquarer()) {
			op->inPortMap("Y", multIn2SigName);
		}
		op->outPortMap("R", output_name);
		for(unsigned i = 1; i < parameters.getOutputWeights().size(); i++){
			op->outPortMap("R" + to_string(i), output_name + to_string(i));
		}

		auto mult = parameters.generateOperator(op, op->getTarget());

		op->vhdl << op->instance(mult, nameOutput.str(), false) <<endl;
		return mult;
	}

	string IntMultiplier::addUID(string name, int blockUID)
	{
		ostringstream s;

		s << name << "_m" << multiplierUid;
		if(blockUID != -1)
			s << "b" << blockUID;
		return s.str() ;
	}

	unsigned int IntMultiplier::getOutputLengthNonZeros(
			BaseMultiplierParametrization const & parameter,
			unsigned int xPos,
			unsigned int yPos,
			unsigned int totalOffset
		)
	{
//        unsigned long long int sum = 0;
		mpfr_t sum;
		mpfr_t two;

		mpfr_inits(sum, two, NULL);

		mpfr_set_si(sum, 1, GMP_RNDN); //init sum to 1 as at the end the log(sum+1) has to be computed
		mpfr_set_si(two, 2, GMP_RNDN);

/*
	cout << "sum=";
	mpfr_out_str(stdout, 10, 0, sum, MPFR_RNDD);
	cout << endl;
	flush(cout);
*/

		for(unsigned int i = 0; i < parameter.getTileXWordSize(); i++){
			for(unsigned int j = 0; j < parameter.getTileYWordSize(); j++){
				if(i + xPos >= totalOffset && j + yPos >= totalOffset){
					if(i + xPos < (wX + totalOffset) && j + yPos < (wY + totalOffset)){
						if(parameter.shapeValid(i,j)){
							//sum += one << (i + j);
							mpfr_t r;
							mpfr_t e;
							mpfr_inits(r, e, NULL);
							mpfr_set_si(e, i+j, GMP_RNDD);
							mpfr_pow(r, two, e, GMP_RNDD);
							mpfr_add(sum, sum, r, GMP_RNDD);
//                            cout << " added 2^" << (i+j) << endl;
						}
						else{
							//cout << "not true " << i << " " << j << endl;
						}
					}
					else{
						//cout << "upper borders " << endl;
					}
				}
				else{
					//cout << "lower borders" << endl;
				}
			}
		}

		mpfr_t length;
		mpfr_inits(length, NULL);
		mpfr_log2(length, sum, GMP_RNDU);
		mpfr_ceil(length,length);

		unsigned long length_ul = mpfr_get_ui(length, GMP_RNDD);
//        cout << " outputLength has a length of " << length_ul << endl;
		return (int) length_ul;
	}

	/*this function computes the amount of zeros at the lsb position (mode 0). This is done by checking for every output bit position, if there is a) an input of the current BaseMultiplier, and b) an input of the IntMultiplier. if a or b or both are false, then there is a zero at this position and we check the next position. otherwise we break. If mode==1, only condition a) is checked */
	/*if mode is 2, only the offset for the bitHeap is computed. this is done by counting how many diagonals have a position, where is an IntMultiplierInput but not a BaseMultiplier input. */
	unsigned int IntMultiplier::getLSBZeros(
			BaseMultiplierParametrization const & param,
			unsigned int xPos,
			unsigned int yPos,
			unsigned int totalOffset,
			int mode
		){
		//cout << "mode is " << mode << endl;
		unsigned int max = min(int(param.getMultXWordSize()), int(param.getMultYWordSize()));
		unsigned int zeros = 0;
		unsigned int mode2Zeros = 0;
		bool startCountingMode2 = false;

		for(unsigned int i = 0; i < max; i++){
			unsigned int steps = i + 1; //for the lowest bit make one step (pos 0,0), second lowest 2 steps (pos 0,1 and 1,0) ...
			//the relevant positions for every outputbit lie on a diogonal line.


			for(unsigned int j = 0; j < steps; j++){
				bool bmHasInput = false;
				bool intMultiplierHasBit = false;
				if(param.shapeValid(j, steps - (j + 1))){
					bmHasInput = true;
				}
				if(bmHasInput && mode == 1){
					return zeros;   //only checking condition a)
				}
				//x in range?
				if((xPos + j >= totalOffset && xPos + j < (wX + totalOffset))){
					//y in range?
					if((yPos + (steps - (j + 1))) >= totalOffset && (yPos + (steps - (j + 1))) < (wY + totalOffset)){
						intMultiplierHasBit = true;
					}
				}
				if(mode == 2 && yPos + xPos + (steps - 1) >= 2 * totalOffset){
					startCountingMode2 = true;
				}
				//cout << "position " << j << "," << (steps - (j + 1)) << " has " << bmHasInput << " and " << intMultiplierHasBit << endl;
				if(bmHasInput && intMultiplierHasBit){
					//this output gets some values. So finished computation and return
					if(mode == 0){
						return zeros;
					}
					else{	//doesnt matter if startCountingMode2 is true or not. mode2Zeros are being incremented at the end of the diagonal
						return mode2Zeros;
					}

				}
			}

			zeros++;
			if(startCountingMode2 == true){
				mode2Zeros++;
			}
		}
		if(mode != 2){
			return zeros;       //if reached, that would mean the the bm shares no area with IntMultiplier
		}
		else{
			return mode2Zeros;
		}
	}

	void IntMultiplier::emulate (TestCase* tc)
	{
		mpz_class svX = tc->getInputValue("X");
		mpz_class svY = tc->getInputValue("Y");
		mpz_class svR;
        if(squarer) svY = svX;
//		cerr << "X : " << svX.get_str(10) << " (" << svX.get_str(2) << ")" << endl;
//		cerr << "Y : " << svY.get_str(10) << " (" << svY.get_str(2) << ")" << endl;


		if(signedIO)
		{
			// Manage signed digits
			mpz_class big1 = (mpz_class{1} << (wX));
			mpz_class big1P = (mpz_class{1} << (wX-1));
			mpz_class big2 = (mpz_class{1} << (wY));
			mpz_class big2P = (mpz_class{1} << (wY-1));

			if(svX >= big1P) {
				svX -= big1;
				//cerr << "X is neg. Interpreted value : " << svX.get_str(10) << endl;
			}

			if(svY >= big2P) {
				svY -= big2;
				//cerr << "Y is neg. Interpreted value : " << svY.get_str(10) << endl;
			}
		}
		svR = svX * svY;
		//cerr << "Computed product : " << svR.get_str() << endl;

		if(negate)
			svR = -svR;

		// manage two's complement at output
		if(svR < 0) {
			svR += (mpz_class(1) << wFullP);
			//cerr << "Prod is neg : encoded value : " << svR.get_str(2) << endl;
		}

		if(wOut>=wFullP) {
			tc->addExpectedOutput("R", svR);
			//cerr << "Exact result is required" << endl;
		}
		else {
			// there is truncation, so this mult should be faithful
			// TODO : this is not faithful but almost faithful :
			// currently only test if error <= ulp, faithful is < ulp
			mpz_class svRtrunc = (svR >> (wFullP-wOut));
			tc->addExpectedOutput("R", svRtrunc);
			if ((svRtrunc << (wFullP - wOut)) != svR) {
				svRtrunc++;
				svRtrunc &= (mpz_class(1) << (wOut)) -1;
				tc->addExpectedOutput("R", svRtrunc);
			}
		}
	}


	void IntMultiplier::buildStandardTestCases(TestCaseList* tcl)
	{
		TestCase *tc;

		mpz_class x, y;

		// 1*1
		x = mpz_class(1);
		y = mpz_class(1);
		tc = new TestCase(this);
		tc->addInput("X", x);
		tc->addInput("Y", y);
		emulate(tc);
		tcl->add(tc);

		// -1 * -1
		x = (mpz_class(1) << wX) -1;
		y = (mpz_class(1) << wY) -1;
		tc = new TestCase(this);
		tc->addInput("X", x);
		tc->addInput("Y", y);
		emulate(tc);
		tcl->add(tc);

		// The product of the two max negative values overflows the signed multiplier
		x = mpz_class(1) << (wX -1);
		y = mpz_class(1) << (wY -1);
		tc = new TestCase(this);
		tc->addInput("X", x);
		tc->addInput("Y", y);
		emulate(tc);
		tcl->add(tc);
	}





	OperatorPtr IntMultiplier::parseArguments(OperatorPtr parentOp, Target *target, std::vector<std::string> &args, UserInterface& ui) {
		int wX,wY, wOut, maxDSP;
		bool signedIO,superTile, use2xk, useirregular, useLUT, useKaratsuba, optiTrunc, minStages, squarer, useGenLUT, useBooth;
		int beamRange = 0;

		ui.parseStrictlyPositiveInt(args, "wX", &wX);
		ui.parseStrictlyPositiveInt(args, "wY", &wY);
		ui.parsePositiveInt(args, "wOut", &wOut);
		ui.parseBoolean(args, "signedIO", &signedIO);
		ui.parseBoolean(args, "superTile", &superTile);
		ui.parseBoolean(args, "use2xk", &use2xk);
		ui.parseBoolean(args, "useirregular", &useirregular);
		ui.parseBoolean(args, "useLUT", &useLUT);
		ui.parseBoolean(args, "useKaratsuba", &useKaratsuba);
		ui.parseBoolean(args, "useGenLUT", &useGenLUT);
		ui.parseBoolean(args, "useBooth", &useBooth);
		ui.parseInt(args, "maxDSP", &maxDSP);
        ui.parseBoolean(args, "optiTrunc", &optiTrunc);
        ui.parseBoolean(args, "minStages", &minStages);
		ui.parsePositiveInt(args, "beamRange", &beamRange);
		ui.parseBoolean(args, "squarer", &squarer);

		return new IntMultiplier(parentOp, target, wX, wY, wOut, signedIO, maxDSP, superTile, use2xk, useirregular, useLUT, useKaratsuba, useGenLUT, useBooth, beamRange, optiTrunc, minStages, squarer);
	}

	template <>
	const OperatorDescription<IntMultiplier> op_descriptor<IntMultiplier> {
		"IntMultiplier", // name
		"A pipelined integer multiplier.  Also uses the global "
		"options: tiling, ilpSolver, etc",
		"BasicInteger",					// category
		"",						// see also
		"wX(int): size of input X; wY(int): size of input Y;\
		 wOut(int)=0: size of the output if you want a truncated multiplier. 0 for full multiplier;\
		 signedIO(bool)=false: inputs and outputs can be signed or unsigned;\
		 maxDSP(int)=-1: limits the number of DSP-Tiles used in Multiplier;\
		 use2xk(bool)=false: if true, attempts to use the 2xk-LUT-Multiplier with relatively high efficiency;\
		 useirregular(bool)=false: if true, attempts to use the irregular-LUT-Multipliers with higher area/lut efficiency than the rectangular versions;\
		 useLUT(bool)=true: if true, attempts to use the LUT-Multipliers for tiling;\
		 useGenLUT(bool)=false: if true, attempts to use the generalized LUT Multipliers, as defined by multiplier_shapes.tiledef;\
		 useBooth(bool)=false: if true, attempts to use LUT-based Booth Arrays;\
		 useKaratsuba(bool)=false: if true, attempts to use rectangular Karatsuba for tiling;\
		 superTile(bool)=false: if true, attempts to use the DSP adders to chain sub-multipliers. This may entail lower logic consumption, but higher latency.;\
		 optiTrunc(bool)=true: if true, considers the Truncation error dynamicly, instead of defining a hard border for tiling, like in the ARITH paper;\
		 minStages(bool)=true: if true, minimizes stages in combined opt. of tiling an comp., otherwise try to find a sol. with less LUTs and more stages;\
		 beamRange(int)=3: range for beam search;\
     squarer[hidden](bool)=false: generate squarer", // This string will be parsed
		""};

	TestList IntMultiplier::unitTest(int testLevel)
	{
		// the static list of mandatory tests
		TestList testStateList;
		vector<pair<string,string>> paramList;

    list<pair<int,int>> wordSizes;
    if(testLevel == TestLevel::QUICK)
    { // The quick tests
      wordSizes = {{1,1},{2,2},{3,3},{4,4},{10,10}};
    }
    else if(testLevel == TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
      wordSizes = {{1,1},{2,2},{3,3},{4,4},{10,10},{16,16},{32,32},{10,20},{20,10}};
    }
    else if(testLevel >= TestLevel::EXHAUSTIVE)
    { // The substantial unit tests
      wordSizes = {{1,1},{2,2},{3,3},{4,4},{8,8},{16,8},{8,16},{13,17},{24,24},{27,41},{35,35},{53,53},{64,64},{10,99},{99,10},{100,100}};
    }

    for (auto wordSizePair : wordSizes)
    {
      int wX = wordSizePair.first;
      int wY = wordSizePair.second;
      for(int sign=0; sign < 2; sign++)
        {
          paramList.push_back(make_pair("wX", to_string(wX)));
          paramList.push_back(make_pair("wY", to_string(wY)));
          paramList.push_back(make_pair("signedIO", sign ? "true" : "false"));
          testStateList.push_back(paramList);

          // same parameters, but truncate (to the size of the smallest input for no particular reason)
          paramList.push_back(make_pair("wOut", to_string(wX)));
          testStateList.push_back(paramList);

          paramList.clear();
        }
    }
		return testStateList;
	}

    mpz_class IntMultiplier::checkTruncationError(list<TilingStrategy::mult_tile_t> &solution, unsigned guardBits, const mpz_class& errorBudget, const mpz_class& constant, int wX, int wY, bool signedIO) {
        std::vector<std::vector<int>> mulAreaI(wX, std::vector<int>(wY,0));

        for(auto & tile : solution) {
            auto &parameters = tile.first;
            auto &anchor = tile.second;
            int xPos = anchor.first;
            int yPos = anchor.second;

            for(int x = (0 <= xPos)?xPos:0; x < (int)((xPos + (int)parameters.getTileXWordSize() < wX)?xPos + parameters.getTileXWordSize():wX); x++){
                for(int y = (0 <= yPos)?yPos:0; y < (int)((yPos + (int)parameters.getTileYWordSize() < wY)?yPos + parameters.getTileYWordSize():wY); y++){
                    if( parameters.shapeValid(x-xPos,y-yPos) || parameters.isSquarer() ){
                        if(1 < std::abs(parameters.getTilingWeight())){
                            mulAreaI[x][y] = mulAreaI[x][y] + ((0 <= parameters.getTilingWeight())?1:(-1));
                            mulAreaI[y][x] = mulAreaI[y][x] + ((0 <= parameters.getTilingWeight())?1:(-1));
                        } else {
                            mulAreaI[x][y] = mulAreaI[x][y] + parameters.getTilingWeight();
                            if(mulAreaI[x][y] > 1 && mulAreaI[y][x] == 0){
                                mulAreaI[x][y]--;
                                mulAreaI[y][x]++;
                            }
                        }
                    }
                }
            }
        }

        mpz_class negTruncError, posTruncError, maxErr = errorBudget+constant;
        negTruncError = mpz_class(0); posTruncError = mpz_class(0);
        for(int y = 0; y < (int)wY; y++){
            for(int x = (int)wX-1; 0 <= x; x--){
                if(mulAreaI[x][y] != 1){
                    // partial product at left and bottom |_ edge count negative in signed case (but not at corner)
                    if(signedIO && ((wX-1 == x) != (wY-1 ==y))){
                        // when they are missing they contribute with a positive error
                        posTruncError += (mpz_class(1)<<(x+y));
                    } else {
                        negTruncError += (mpz_class(1)<<(x+y));
                    }
                }
                cout << ((mulAreaI[x][y] == 1) ? 1 : 0);
            }
            cout << endl;
        }

        if(max(negTruncError,posTruncError) <= maxErr){
            cout << "OK: actual truncation error=" << max(negTruncError,posTruncError) << " is smaller than the max. permissible error=" << maxErr << " by " << maxErr-max(negTruncError,posTruncError) << "." << endl;
        } else {
            cout << "ERROR: actual truncation error=" << max(negTruncError,posTruncError) << " is larger than the max. permissible error=" << maxErr << " by " << max(negTruncError,posTruncError)-maxErr << "." << endl;
        }
        return max(negTruncError,posTruncError);
	}

	int IntMultiplier::calcBitHeapLSB(list<TilingStrategy::mult_tile_t> &solution, unsigned guardBits, const mpz_class& errorBudget, const mpz_class& constant, const mpz_class& actualTruncError, int wX, int wY){
	    int col=0, nBits = 0;
	    std::vector<std::vector<int>> mulAreaI(wX, std::vector<int>(wY,0));

	    for(auto & tile : solution) {
	        auto &parameters = tile.first;
	        auto &anchor = tile.second;
	        int xPos = anchor.first;
	        int yPos = anchor.second;

	        for(int x = (0 <= xPos)?xPos:0; x < (int)((xPos + parameters.getTileXWordSize() < wX)?xPos + parameters.getTileXWordSize():wX); x++){
	            for(int y = (0 <= yPos)?yPos:0; y < (int)((yPos + parameters.getTileYWordSize() < wY)?yPos + parameters.getTileYWordSize():wY); y++){
	                if( parameters.shapeValid(x-xPos,y-yPos) || parameters.isSquarer() ){
	                    if(1 < std::abs(parameters.getTilingWeight())){
	                        mulAreaI[x][y] = mulAreaI[x][y] + ((0 <= parameters.getTilingWeight())?1:(-1));
	                        mulAreaI[y][x] = mulAreaI[y][x] + ((0 <= parameters.getTilingWeight())?1:(-1));
	                    } else {
	                        mulAreaI[x][y] = mulAreaI[x][y] + parameters.getTilingWeight();
	                        if(mulAreaI[x][y] > 1 && mulAreaI[y][x] == 0){
	                            mulAreaI[x][y]--;
	                            mulAreaI[y][x]++;
	                        }
	                    }
	                }
	            }
	        }
	    }

        mpz_class error, weight, cnew = constant;
        do{
            nBits = 0;
            error = 0;
            cout << " min weight=" << col << endl;
            mpz_pow_ui(weight.get_mpz_t(), mpz_class(2).get_mpz_t(), col);
            for(int x = 0; x <= col && x < (int)wX; x++){
                for(int y = 0; x+y <= col && y < (int)wY; y++){
                    error += (mulAreaI[x][y] == 1)?weight:0;
                    nBits += (mulAreaI[x][y] == 1)?1:0;
                }
            }
            cnew -= (((cnew & weight) > 0)?weight:0);     //Remove bit from error re-centering constant if the column with the corresponding weight is removed from bitheap
            cout << "trying to prune " << nBits << " bits with weight 2^" << col << " error is "  << error << " additional permissible error is " << errorBudget + cnew - actualTruncError << " recentering const=" << cnew << endl;
            col++;
        } while(actualTruncError + error < errorBudget + cnew || 0 == error);
        col--;
        cout << "min req weight is=" << col << endl;
        return col;
    }

    void IntMultiplier::createFigures(TilingStrategy *tilingStrategy) {
	    ofstream texfile;
			int wTrunc=0; // TODO obviously wrong, the parameter is called wTrunc but was called below with the IntMultiplier attribute wOut
	    texfile.open("multiplier_tiling.tex");
	    if ((texfile.rdstate() & ofstream::failbit) != 0) {
	        cerr << "Error when opening multiplier_tiling.tex file for output. Will not print tiling configuration."
	        << endl;
	    } else {
				tilingStrategy->printSolutionTeX(texfile, false); // was wOut
	        texfile.close();
	    }

	    texfile.open("multiplier_tiling.svg");
	    if ((texfile.rdstate() & ofstream::failbit) != 0) {
	        cerr << "Error when opening multiplier_tiling.svg file for output. Will not print tiling configuration."
	        << endl;
	    } else {
	        tilingStrategy->printSolutionSVG(texfile, false);// was wOut
	        texfile.close();
	    }

	    texfile.open("multiplier_shape.tex");
	    if ((texfile.rdstate() & ofstream::failbit) != 0) {
	        cerr << "Error when opening multiplier_shape.tex file for output. Will not print tiling configuration."
	        << endl;
	    } else {
	        tilingStrategy->printSolutionTeX(texfile, true);// was wOut
	        texfile.close();
	    }

	    
	}

}
