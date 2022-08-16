#ifndef BaseMultiplierDSP_HPP
#define BaseMultiplierDSP_HPP

#include <iostream>
#include <string>

#include <gmp.h>
#include <gmpxx.h>

#include "flopoco/IntMult/BaseMultiplierCategory.hpp"
#include "flopoco/Operator.hpp"
#include "flopoco/Tables/Table.hpp"
#include "flopoco/Target.hpp"

namespace flopoco {


    class BaseMultiplierDSP : public BaseMultiplierCategory
    {

	public:
		BaseMultiplierDSP(
				int maxWidthBiggestWord,
				int maxWidthSmallestWord,
				int deltaWidthSigned
			) : BaseMultiplierCategory{
				maxWidthBiggestWord,
				maxWidthSmallestWord,
				false,
                false,
				-1,
				"BaseMultiplierDSP"
		}{}

        BaseMultiplierDSP(
                int wX,
                int wY,
                bool isSignedX,
                bool isSignedY
        ) : BaseMultiplierCategory{
                wX,
                wY,
                isSignedX,
                isSignedY,
                -1,
                "BaseMultiplierDSP"
        }{}

        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO);
        int getDSPCost() const final {return 1;}

        bool signSupX() override{return true;}
        bool signSupY() override{return true;}

		Operator* generateOperator(
				Operator *parentOp,
				Target *target,
				Parametrization const & parameters
			) const final;

    private:
        bool flipXY;
        bool pipelineDSPs;
	};
}
#endif
