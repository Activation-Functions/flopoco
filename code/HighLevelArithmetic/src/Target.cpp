/*
  The abstract class that models different target FGPAs for FloPoCo.
  Classes for real chips (in the Targets directory) inherit from this one.

  Author : Florent de Dinechin

  This file is part of the FloPoCo project
  developed by the Arenaire team at Ecole Normale Superieure de Lyon

  Initial software.
  Copyright © ENS-Lyon, INRIA, CNRS, UCBL,
  2008-2011.
  All rights reserved.

 */

#include "flopoco/Target.hpp"


using namespace std;


namespace flopoco{

	extern int verbose;

	Target::Target()   {
            useTargetOptimizations_=true;
			lutInputs_         = 4;
			hasHardMultipliers_= true;
			hasFastLogicTernaryAdders_ = false;
			id_                = "generic";

			pipeline_          = true;
			useClockEnable_       = false;
			nameSignalByCycle_       = false;
			frequency_         = 400000000.;
			useHardMultipliers_= true;
			unusedHardMultThreshold_=0.5;
			registerLargeTables_=true; 
			tableCompression_=true;
			ilpTimeout_=0;
			generateFigures_=false;
		}

	Target::~Target()
	{
	}


	// vector<Operator*> *  Target::getGlobalOpListRef(){
	// 	return & globalOpList;
	// }

	string Target::getID(){
		return id_;
	}

	string Target::getVendor(){
			return vendor_;
		}


	bool Target::isPipelined() {
		return (frequency_!=0);
	}

	void Target::setClockEnable(bool val) {
		useClockEnable_=val;
	}

	void Target::setNameSignalByCycle(bool val) {
		nameSignalByCycle_=val;
	}

	bool Target::useClockEnable(){
		return useClockEnable_;
	}

	bool Target::useNameSignalByCycle(){
		return nameSignalByCycle_;
	}

	int Target::lutInputs() {
		return lutInputs_;
	}

	int Target::maxLutInputs() {
		// Default behavior : return the basic lut size
		return lutInputs_;
	}

	double Target::frequency(){
		return frequency_;
	}


	double Target::frequencyMHz(){
		return floor(frequency_/1000000);
	}

	double Target::normalizedFrequency(){
		return frequencyMHz()/maxFrequencyMHz_;
	}
	void Target::setFrequency(double f){
		frequency_ = f;
	}

	void Target::setUseHardMultipliers(bool v){
		hasHardMultipliers_ = v;
	}

	void Target::setPlainVHDL(bool v){
		plainVHDL_ = v;
	}

	bool Target::plainVHDL(){
		return plainVHDL_;
	}

	bool  Target::generateFigures(){
		return generateFigures_;
	}

    void  Target::setGenerateFigures(bool b)
    {
      generateFigures_ = b;
    }

	bool  Target::registerLargeTables(){
		return registerLargeTables_;
	}

    void  Target::setRegisterLargeTables(bool b)
    {
      registerLargeTables_ = b;
    }
	
	bool  Target::tableCompression(){
		return tableCompression_;
	}

    void  Target::setTableCompression(bool b)
    {
      tableCompression_ = b;
    }

    bool  Target::useTargetOptimizations()
    {
      return useTargetOptimizations_;
    }


    void Target::setUseTargetOptimizations(bool b)
    {
      useTargetOptimizations_ = b;
    }

	void Target::setCompressionMethod(string compression)
	{
		compression_ = compression;
	}

	string Target::getCompressionMethod()
	{
		return compression_;
	}

	void Target::setILPSolver(string ilpSolverName)
	{
		ilpSolverName_ = ilpSolverName;
	}

	string Target::getILPSolver()
	{
		return ilpSolverName_;
	}

	void Target::setILPTimeout(int ilpTimeout)
	{
		ilpTimeout_ = ilpTimeout;
	}

	int Target::getILPTimeout()
	{
		return ilpTimeout_;
	}

	void Target::setTilingMethod(string method)
	{
		tiling_ = method;
	}

	string Target::getTilingMethod()
	{
		return tiling_;
	}



    bool Target::hasHardMultipliers(){
		return hasHardMultipliers_ ;
	}

	bool Target::useHardMultipliers(){
		return (hasHardMultipliers_ && useHardMultipliers_) ;
	}

	void Target::setUnusedHardMultThreshold(float v) {
		unusedHardMultThreshold_=v;
	}

	float Target::unusedHardMultThreshold() {
		return unusedHardMultThreshold_;
	}


//	double Target::logicDelay(int inputs){
//		double unitDelay = lutDelay();
//		if(inputs <= lutInputs())
//			return unitDelay;
//		else
//			return unitDelay * (inputs -lutInputs() + 1);
//	}


	
	
	bool Target::hasFastLogicTernaryAdders(){
		return hasFastLogicTernaryAdders_ ;
	}

	bool Target::worthUsingDSP(int wX, int wY){
		// Default random setting, should be overloaded after a bit of experimenting
		int threshold = multYInputs_ >> 1; // the smallest dimension in case of asymmetry
		if(wX < threshold && wY < threshold)
			return false;
		else
			return true;
	}


	// A heuristic that is equally bad  for any target
	int Target::plainMultDepth(int wX, int wY){
		int cycles;
		if(hasHardMultipliers_) {
			int nX1 = ceil( (double)wX/(double)multXInputs_ );
			//int nY1 = ceil( (double)wY/(double)multYInputs_ );
			int nX2 = ceil( (double)wY/(double)multXInputs_ );
			//int nY2 = ceil( (double)wX/(double)multYInputs_ );
			/*int nX,nY;
			if(nX1*nY1>nX2*nY2) {
				nX=nX2;
				nY=nY2;
			} else {
				nX=nX1;
				nY=nY1;
			}*/
			int adds=nX1+nX2;
			cycles = ( DSPMultiplierDelay() + adds* DSPAdderDelay() ) * frequency_;
		}
		else {
			// assuming a plain block multiplier, critical path through wX+wY full adders. Could be refined.
			double init, carry;
			init = adderDelay(0);
			carry = (adderDelay(100) - init) /100; 
			cycles = (init + (wX+wY)*carry) *frequency_;
		}
		cout << "> Target: Warning: using generic Target::plainMultDepth(); pipelining a "<<wX<<"x"<<wY<< " multiplier in " << cycles << " cycles using a gross estimate of the target" << endl;
		return cycles;
	}

	
	double Target::tableDelay(int wIn_, int wOut_, bool logicTable_){
		cout << "Warning: using the generic Target::tableDelay(); it should be overloaded in your target instead" << endl;
		return 2e-9;
	}

	double Target::cycleDelay() {
		cout << "Warning: using the generic Target::cycleDelay() with a Target which is not ManualPipeline" << endl;
		return 0.;
	}



	int Target::tableDepth(int wIn, int wOut){
		cout << "Warning: using the generic Target::tableDepth(); pipelining using a gross estimate of the target" << endl;
		return 2; // TODO
	}


	vector<pair<int,int>> Target::possibleDSPConfigs() {
		return possibleDSPConfig_;
	}

	vector<bool> Target::whichDSPCongfigCanBeUnsigned() {
		return whichDSPCongfigCanBeUnsigned_;
	}

	void Target::getMaxDSPWidths(int &x, int &y, bool sign){
		pair<int,int> sizes = possibleDSPConfig_.back();
		int dec; // remove 1 if unsigned and fullsize unsigned not supported
		if(sign ||  whichDSPCongfigCanBeUnsigned_.back())
			dec=0;
		else
			dec=1;
		x = sizes.first - dec;
		y = sizes.second - dec;
		//cout << "Target::getMaxDSPWidths for " << (sign?"":"un") << "signed mult returns x="<<x <<" and y=" << y<<endl << "get rid of this annoying message in Target.cpp once it is clear it works" << endl; 
	}


    double Target::getBitHeapCompressionCostperBit(){
        if(vendor_ == "Xilinx"){
            return 0.65;
        }
        //TODO: Add entry for Intel/Altera when the cost is known.
        return 0.65;
	}

}
