#include "flopoco/IntMult/MultiplierTileCollection.hpp"
#include "flopoco/IntMult/BaseMultiplierDSP.hpp"
#include "flopoco/IntMult/BaseMultiplierLUT.hpp"
#include "flopoco/IntMult/BaseMultiplierXilinx2xk.hpp"
#include "flopoco/IntMult/BaseMultiplierXilinxGeneralizedLUT.hpp"
#include "flopoco/IntMult/BaseMultiplierBoothArrayXilinx.hpp"
#include "flopoco/IntMult/BaseMultiplierIrregularLUTXilinx.hpp"
#include "flopoco/IntMult/BaseMultiplierDSPSuperTilesXilinx.hpp"
#include "flopoco/IntMult/BaseMultiplierDSPKaratsuba.hpp"
#include "flopoco/IntMult/BaseSquarerLUT.hpp"

using namespace std;
namespace flopoco {

	MultiplierTileCollection::MultiplierTileCollection(Target *target, BaseMultiplierCollection *bmc, int mult_wX, int mult_wY, bool superTile, bool use2xk, bool useirregular, bool useLUT, bool useDSP, bool useKaratsuba, bool useGenLUT, bool useBooth, bool squarer, bool varSizeDSP):squarer{squarer} {
        //cout << bmc->size() << endl;
        int tilingWeights[4] = {1, -1, 2, -2};
        for(int w = 0; w < ((squarer)?4:1); w++){

        	if(useBooth){
        		variableTileOffset = 2;
        		for(int x = variableTileOffset; x <= mult_wX; x++) {
                                for(int y = variableTileOffset; y <= 5; y++) {
                                        addBaseTile(target, new BaseMultiplierBoothArrayXilinx(x, y), tilingWeights[w]);
                                }
                        }
                        for(int x = variableTileOffset; x <= 5; x++) {
                                for(int y = variableTileOffset; y <= mult_wY; y++) {
                                        addBaseTile(target, new BaseMultiplierBoothArrayXilinx(x, y), tilingWeights[w]);
                                }
                        }
        	}

            if(useGenLUT){      //Generalized Xilinx LUT multiplier
                std::vector<vector<pair<int,int>>> shapes;
                std::ifstream multdef;
                multdef.open("./multiplier_shapes.tiledef");
                if(multdef.is_open()) {
                    std::string line;
                    while (std::getline(multdef, line)) {
                        std::vector <pair<int,int>> coords;
                        int next;
                        while(0 <= (next = line.find(";"))){
                            std::string coordinate = line.substr(0, next);
                            line = line.substr(next+1, line.length());
                            int x = stoi(coordinate.substr(0, coordinate.find(",")));
                            int y = stoi(coordinate.substr(coordinate.find(",")+1, coordinate.length()));
                            coords.push_back(make_pair(x,y));
                        }
                        if(0 < coords.size()) shapes.push_back(coords);
                    }
                    multdef.close();
                    vector<vector<vector<int>>> shape_coverage;
                    for(int i=0; i < shapes.size(); i++){
                        std::vector<vector<int>> coverage(shapes[i][0].first,vector<int>(shapes[i][0].second));
                        for(int j=1; j<shapes[i].size(); j++){
                            coverage[shapes[i][j].first][shapes[i][j].second] = 1;
                        }
                        shape_coverage.push_back(coverage);
                        addBaseTile(target, new BaseMultiplierXilinxGeneralizedLUT(shape_coverage.back()), tilingWeights[w]);
                    }
                } else {
                    cerr << "Error when opening multiplier_shapes.tiledef file for input." << endl;
                }

            }

            if(useDSP) {
                if(!varSizeDSP){
                    addBaseTile(target, new BaseMultiplierDSP(24, 17, 1), tilingWeights[w]);
                    addBaseTile(target, new BaseMultiplierDSP(17, 24, 1), tilingWeights[w]);
                } else {
                    for(int i = 1; i <= ((mult_wY<17)?mult_wY:17); i++) {
                        addBaseTile(target, new BaseMultiplierDSP(((mult_wX<24)?mult_wX:24), i, 1), tilingWeights[w]);
                    }
                    for(int i = 1; i <= ((mult_wX<17)?mult_wX:17); i++) {
                        addBaseTile(target, new BaseMultiplierDSP(i, ((mult_wY<24)?mult_wY:24), 1), tilingWeights[w]);
                    }
                }
            }

            if(useLUT) {
                addBaseTile(target, new BaseMultiplierLUT(3, 3), tilingWeights[w]);
                addBaseTile(target, new BaseMultiplierLUT(2, 3), tilingWeights[w]);
                addBaseTile(target, new BaseMultiplierLUT(3, 2), tilingWeights[w]);
                addBaseTile(target, new BaseMultiplierLUT(1, 2), tilingWeights[w]);
                addBaseTile(target, new BaseMultiplierLUT(2, 1), tilingWeights[w]);
                addBaseTile(target, new BaseMultiplierLUT(1, 1), tilingWeights[w]);
            }

            if(superTile){
                for(int i = 1; i <= 12; i++) {
                    addSuperTile(target, new BaseMultiplierDSPSuperTilesXilinx((BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE) i), tilingWeights[w]);
                }
            }

            if(use2xk){
                variableTileOffset = 4;
                for(int x = variableTileOffset; x <= mult_wX; x++) {
                    addVariableXTile(target, new BaseMultiplierXilinx2xk(x, 2), tilingWeights[w]);
                }

                for(int y = variableTileOffset; y <= mult_wY; y++) {
                    addVariableYTile(target, new BaseMultiplierXilinx2xk(2, y), tilingWeights[w]);
                }
            }

            if(useirregular){
                for(int i = 1; i <= 8; i++) {
                    addBaseTile(target, new BaseMultiplierIrregularLUTXilinx((BaseMultiplierIrregularLUTXilinx::TILE_SHAPE) i), tilingWeights[w]);
                }
            }

            if(useKaratsuba) {
                unsigned int min = std::min((mult_wX - 16) / 48, (mult_wY - 24) / 48);
                for(unsigned int i = 0; i <= min; i++) {
                    addBaseTile(target, new BaseMultiplierDSPKaratsuba(i, 16, 24), tilingWeights[w]);
                }
            }

            if(squarer && tilingWeights[w] != (-2) && tilingWeights[w] != 2){
                for(int i = 1; i <= 6; i++) {
                    addBaseTile(target, new BaseSquarerLUT(i), tilingWeights[w]);
                }
            }
        }
/*        for(int i = 0; i < (int)bmc->size(); i++)
        {
            cout << bmc->getBaseMultiplier(i).getType() << endl;

            if( (bmc->getBaseMultiplier(i).getType().compare("BaseMultiplierDSPSuperTilesXilinx")) == 0){
                for(int i = 1; i <= 12; i++) {
                    MultTileCollection.push_back(
                            new BaseMultiplierDSPSuperTilesXilinx((BaseMultiplierDSPSuperTilesXilinx::TILE_SHAPE) i));
                }
            }

            if( (bmc->getBaseMultiplier(i).getType().compare("BaseMultiplierIrregularLUTXilinx")) == 0){
                for(int i = 1; i <= 8; i++) {
                    MultTileCollection.push_back(
                            new BaseMultiplierIrregularLUTXilinx((BaseMultiplierIrregularLUTXilinx::TILE_SHAPE) i));
                }
            }

        }
*/
//        cout << MultTileCollection.size() << endl;
    }

    void  MultiplierTileCollection::addBaseTile(Target *target, BaseMultiplierCategory *mult, int tilingWeight) {
        mult->setTarget(target);
        mult->setTilingWeight(tilingWeight);
        MultTileCollection.push_back(mult);
        BaseTileCollection.push_back(mult);
    }

    void  MultiplierTileCollection::addSuperTile(Target *target,BaseMultiplierCategory *mult, int tilingWeight) {
        mult->setTarget(target);
        mult->setTilingWeight(tilingWeight);
        MultTileCollection.push_back(mult);
        SuperTileCollection.push_back(mult);
    }

    void  MultiplierTileCollection::addVariableXTile(Target *target,BaseMultiplierCategory *mult, int tilingWeight) {
        mult->setTarget(target);
        mult->setTilingWeight(tilingWeight);
        MultTileCollection.push_back(mult);
        VariableXTileCollection.push_back(mult);
    }

    void  MultiplierTileCollection::addVariableYTile(Target *target,BaseMultiplierCategory *mult, int tilingWeight) {
        mult->setTarget(target);
        mult->setTilingWeight(tilingWeight);
        MultTileCollection.push_back(mult);
        VariableYTileCollection.push_back(mult);
    }

    BaseMultiplierCategory* MultiplierTileCollection::superTileSubtitution(vector<BaseMultiplierCategory*> mtc, int rx1, int ry1, int lx1, int ly1, int rx2, int ry2, int lx2, int ly2){

        for(int i = 0; i < (int)mtc.size(); i++)
        {
            if(mtc[i]->getDSPCost() == 2){
                int id = mtc[i]->isSuperTile(rx1, ry1, lx1, ly1, rx2, ry2, lx2, ly2);
                if(id){
                    return mtc[i+id];
                }
            }
        }
        return nullptr;
    }

}
