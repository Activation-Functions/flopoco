#include "flopoco/IntMult/TilingStrategyCSV.hpp"
#include "flopoco/IntMult/BaseMultiplierIrregularLUTXilinx.hpp"
#include "flopoco/IntMult/BaseMultiplierXilinx2xk.hpp"
#include "flopoco/IntMult/LineCursor.hpp"
#include "flopoco/IntMult/NearestPointCursor.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"

#include <cstdlib>
#include <ctime>


namespace flopoco {
	TilingStrategyCSV::TilingStrategyCSV(
			unsigned int wX,
			unsigned int wY,
			unsigned int wOut,
			bool signedIO,
			BaseMultiplierCollection* bmc,
			base_multiplier_id_t prefered_multiplier,
			float occupation_threshold,
			size_t maxPrefMult,
			bool useIrregular,
			bool use2xk,
			bool useSuperTiles,
			bool useKaratsuba,
			MultiplierTileCollection tiles_,
            unsigned guardBits,
            unsigned keepBits):TilingStrategy(wX, wY, wOut, signedIO, bmc),
								prefered_multiplier_{prefered_multiplier},
								occupation_threshold_{occupation_threshold},
								max_pref_mult_{maxPrefMult},
								useIrregular_{useIrregular},
								use2xk_{use2xk},
								useSuperTiles_{useSuperTiles},
								useKaratsuba_{useKaratsuba},
								tiles{tiles_.MultTileCollection},
                                guardBits_{guardBits},
                                keepBits_{keepBits}
	{
	}

	void TilingStrategyCSV::solve() {
		
		double cost = 0.0;
		unsigned int area = 0;
		unsigned int usedDSPBlocks = 0;
		//only one state, base state is also current state

		std::vector <triplet<int,int,int>> placements;
		std::ifstream multdef;
		multdef.open("./tiling_solution.csv");
		if(multdef.is_open()) {
		    std::string line;
		    while (std::getline(multdef, line)) {

		        int next;
		        if(0 <= (next = line.find(";"))){
		            std::string placement = line.substr(0, next);
		            int t = stoi(placement.substr(0, placement.find(",")));
		            placement = placement.substr(placement.find(",")+1, placement.length());
		            int x = stoi(placement.substr(0, placement.find(",")));
		            placement = placement.substr(placement.find(",")+1, placement.length());
		            int y = stoi(placement.substr(0, placement.find(",")));
		            placements.push_back(make_triplet(t,x,y));
		            cout << "t=" << t << " x=" << x << " y=" << y << endl;
		            
		            cost += (double) tiles[t]->getLUTCost(x, y, wX, wY, signedIO);
		            //own_lut_cost += tiles[t]->ownLUTCost(x, y, wX, wY, signedIO);
		            usedDSPBlocks += (double) tiles[t]->getDSPCost();
		            auto coord = make_pair(x, y);
		            solution.push_back(make_pair(
		                    tiles[t]->getParametrisation().tryDSPExpand(x, y, wX, wY, signedIO),
		                    coord));
		        }
		    }
		    multdef.close();
		} else {
		    cerr << "Error when opening tiling_solution.csv file for input." << endl;
		}

		//exit(1);

		cout << "Total cost: " << cost << " " << usedDSPBlocks << endl;
		//cout << "Total area: " << area << endl;

	}
}
