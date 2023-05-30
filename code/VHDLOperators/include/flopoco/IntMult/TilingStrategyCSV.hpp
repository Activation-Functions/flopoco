#ifndef FLOPOCO_TILINGSTRATEGYCSV_HPP
#define FLOPOCO_TILINGSTRATEGYCSV_HPP

#include <queue>

#include "TilingStrategy.hpp"
#include "Field.hpp"
#include "MultiplierTileCollection.hpp"

namespace flopoco {
    class TilingStrategyCSV : public TilingStrategy {
    public:
        TilingStrategyCSV(
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
                MultiplierTileCollection tiles,
                unsigned guardBits,
                unsigned keepBits);
        virtual ~TilingStrategyCSV() { }

        virtual void solve();

    protected:
        vector<BaseMultiplierCategory*> tiles;
        base_multiplier_id_t prefered_multiplier_;
        float occupation_threshold_;
        size_t max_pref_mult_;
        bool useIrregular_;
        bool use2xk_;
        bool useSuperTiles_;
        bool useKaratsuba_;

        vector<BaseMultiplierCategory*> tiles_;
        bool truncated_;
        //unsigned int truncatedRange_;
        unsigned prodsize_;
        unsigned guardBits_;
        unsigned keepBits_;

        
    };
}
#endif