#ifndef BaseMultiplierLUT_HPP
#define BaseMultiplierLUT_HPP

#include <iostream>
#include <string>

#include <gmp.h>
#include <gmpxx.h>

#include "flopoco/IntMult/BaseMultiplierCategory.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/Target.hpp"

namespace flopoco {
    class BaseMultiplierLUT : public BaseMultiplierCategory
    {

	public:
        BaseMultiplierLUT(int maxSize, double lutPerOutputBit):
                BaseMultiplierCategory{
                        maxSize,
                        maxSize,
                        false,
                        false,
                        -1,
                        "BaseMultiplierLUT"
                }, lutPerOutputBit_{lutPerOutputBit}
        {}

        BaseMultiplierLUT(
                int wX,
                int wY
        ) : BaseMultiplierCategory{
                    wX,
                    wY,
                    false,
                    false,
                    -1,
                    "BaseMultiplierLUT_" + string(1,((char) wX) + '0') + "x" + string(1,((char) wY) + '0')
        }{}

            int getDSPCost() const override { return 0; }
            double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) override;
            int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) override;
            bool signSupX() override {return true;}
            bool signSupY() override {return true;}

			Operator *generateOperator(
					Operator *parentOp,
					Target *target,
					Parametrization const & parameters
				) const final;
	private:
			double lutPerOutputBit_;
	};
}
#endif
