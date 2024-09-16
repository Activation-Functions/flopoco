#ifndef ManualPipeline_HPP
#define ManualPipeline_HPP

#include "flopoco/Target.hpp"

namespace flopoco{

	/** Class for representing an ManualPipeline target */
	class ManualPipeline : public Target
	{
	public:
		/** The default constructor. */  
		ManualPipeline();
		/** The destructor */
		~ManualPipeline();
		/** overloading the virtual functions of Target
		 * @see the target class for more details 
		 */

		// Overloading virtual methods of Target 
		double logicDelay(int inputs);

		double adderDelay(int size, bool addRoutingDelay=true);

		double eqComparatorDelay(int size);
		double ltComparatorDelay(int size);
		double eqConstComparatorDelay(int size);
		
		double lutDelay();
		double addRoutingDelay(double d);
		double fanoutDelay(int fanout = 1);
		double ffDelay();
		long   sizeOfMemoryBlock();
		double tableDelay(int wIn, int wOut, bool logicTable);
		double cycleDelay();

		// The following is a list of methods that are not totally useful: TODO in Target.hpp
		double adder3Delay(int size){return 0;}; // currently irrelevant for Xilinx
		double carryPropagateDelay(){return 0;};
		double DSPMultiplierDelay(){ 
			return 0;
			// TODO: return DSPMultiplierDelay_;
		}
		double DSPAdderDelay(){
			// TODO: return DSPAdderDelay_;
			return 0;
		}
		double DSPCascadingWireDelay(){
			// return DSPCascadingWireDelay_;
			return 0;
		}
		double DSPToLogicWireDelay(){
			// return DSPToLogicWireDelay_;
			return 0;
		}
		int    getEquivalenceSliceDSP();
		
		bool   suggestSlackSubaddSize(int &x, int wIn, double slack) {return false;}; // TODO
		bool   suggestSlackSubadd3Size(int &x, int wIn, double slack){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSubaddSize(int &x, int wIn);
		bool   suggestSubadd3Size(int &x, int wIn){return 0;}; // currently irrelevant for Xilinx
		bool   suggestSlackSubcomparatorSize(int &x, int wIn, double slack, bool constant);// used in IntAddSubCmp/IntComparator.cpp:
		double LogicToDSPWireDelay(){ return 0;} //TODO

		double RAMDelay() { return 0.; }
		double LogicToRAMWireDelay() { return 0.; }
		double lutConsumption(int lutInputSize);

		/** 
		 * The four 6-LUTs of a slice can be combined without routing into a 
		 * 8-LUT
		 */
		int maxLutInputs() { return 8; } //TODO is it useful in compressor
#if 0
		
		double RAMToLogicWireDelay() { return RAMToLogicWireDelay_; }
		

		bool   suggestSubmultSize(int &x, int &y, int wInX, int wInY);
		
		int    getIntMultiplierCost(int wInX, int wInY);
		int    getNumberOfDSPs();
		int    getIntNAdderCost(int wIn, int n);	
#endif
		

	};

}
#endif
