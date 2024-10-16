#ifndef FLOPOCO_MULTIPLIERTILECOLLECTION_HPP
#define FLOPOCO_MULTIPLIERTILECOLLECTION_HPP
#include "BaseMultiplierCategory.hpp"
#include "BaseMultiplierCollection.hpp"

namespace flopoco {
    class MultiplierTileCollection {

    public:
    	MultiplierTileCollection(Target *target, BaseMultiplierCollection* bmc, int mult_wX, int mult_wY, bool superTile, bool use2xk, bool useirregular, bool useLUT, bool useDSP, bool useKaratsuba, bool useGenLUT, bool useBooth, bool squarer, bool varSizeDSP=false);
        vector<BaseMultiplierCategory*> MultTileCollection;
        vector<BaseMultiplierCategory*> BaseTileCollection;
        vector<BaseMultiplierCategory*> VariableYTileCollection;
        vector<BaseMultiplierCategory*> VariableXTileCollection;
        vector<BaseMultiplierCategory*> SuperTileCollection;
        unsigned int variableTileOffset;
        bool squarer;

        static BaseMultiplierCategory *
        superTileSubtitution(vector<BaseMultiplierCategory*> mtc, int rx1, int ry1, int lx1, int ly1, int rx2, int ry2, int lx2, int ly2);

    private:
        void addBaseTile(Target *target, BaseMultiplierCategory* mult, int tilingWeight);
        void addVariableXTile(Target *target, BaseMultiplierCategory* mult, int tilingWeight);
        void addVariableYTile(Target *target, BaseMultiplierCategory* mult, int tilingWeight);
        void addSuperTile(Target *target, BaseMultiplierCategory* mult, int tilingWeight);
    };
}
#endif //FLOPOCO_MULTIPLIERTILECOLLECTION_HPP
