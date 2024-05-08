add_flopocolib_src(
    BaseMultiplierBoothArrayXilinx.cpp
    BaseMultiplierCategory.cpp
    BaseMultiplierCollection.cpp
    BaseMultiplier.cpp
    BaseMultiplierDSP.cpp
    BaseMultiplierDSPKaratsuba.cpp
    BaseMultiplierDSPSuperTilesXilinx.cpp
    BaseMultiplierIrregularLUTXilinx.cpp
    BaseMultiplierXilinxGeneralizedLUT.cpp
    BaseMultiplierLUT.cpp
    BaseMultiplierXilinx2xk.cpp
    BaseSquarerLUT.cpp
    DSPBlock.cpp
    Field.cpp
    FixMultAdd.cpp
    #FormalBinaryProduct.cpp
    #GenericBinaryPolynomial.cpp
    #IntKaratsuba.cpp
    IntKaratsubaRectangular.cpp
    IntMultiplier.cpp
    IntMultiplierLUT.cpp
    #IntPower.cpp
    IntSquarer.cpp
    #MultiplierBlock.cpp
    MultiplierTileCollection.cpp
    NearestPointCursor.cpp
    TargetBaseMultSpecialisation.cpp
    #Tiling.cpp
     TilingStrategyBasicTiling.cpp
     TilingStrategy.cpp
# All the other tiling strategies temporarily disabled by Florent while bringing up virtual multipliers 
#    TilingStrategyBeamSearch.cpp
#    TilingStrategyGreedy.cpp
#    TilingStrategyOptimalILP.cpp
#    TilingStrategyXGreedy.cpp
#    TilingStrategyCSV.cpp
#    TilingAndCompressionOptILP.cpp
)

target_include_directories(FloPoCoLib PRIVATE ${flopocolib_header_root}/IntMult)
