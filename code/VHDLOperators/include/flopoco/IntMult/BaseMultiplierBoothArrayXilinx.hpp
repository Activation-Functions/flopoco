#ifndef BaseMultiplierBoothArrayXilinx_HPP
#define BaseMultiplierBoothArrayXilinx_HPP

#include <string>
#include <iostream>
#include <string>

#include <gmp.h>
#include <gmpxx.h>

#include "flopoco/Target.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/IntMult/BaseMultiplierCategory.hpp"
#include "flopoco/UserInterface.hpp"

namespace flopoco {
    class BaseMultiplierBoothArrayXilinx : public BaseMultiplierCategory
    {
	public:
        BaseMultiplierBoothArrayXilinx(int wX, int wY):
			BaseMultiplierCategory{
            wX,
            wY,
            false,
            false,
            -1,
            "BaseMultiplierBoothArrayXilinx_" + to_string(wX) + "x" +  to_string(wY)}
		{}

        int getDSPCost() const override { return 0; }
        bool isVariable() const override { return true; }
        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) override;
        int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) override;

		/** Factory method */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui);
        /** Register the factory */
        static void registerFactory();
        static TestList unitTest(int testLevel);

		Operator *generateOperator(
				Operator *parentOp,
				Target *target,
				Parametrization const & parameters
			) const final;
	};

    class BaseMultiplierBoothArrayXilinxOp : public Operator
    {
    public:
		BaseMultiplierBoothArrayXilinxOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, int wX, int wY, int wAcc =0);

        void emulate(TestCase* tc);
    private:
        bool xIsSigned, yIsSigned;
        int wX, wY, wAcc;
    };

}
#endif