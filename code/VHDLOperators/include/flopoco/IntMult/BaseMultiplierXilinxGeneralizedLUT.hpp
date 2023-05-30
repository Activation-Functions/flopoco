#include "flopoco/Operator.hpp"
#include "BaseMultiplierCategory.hpp"

namespace flopoco
{

    class BaseMultiplierXilinxGeneralizedLUT : public BaseMultiplierCategory
    {
    public:
        BaseMultiplierXilinxGeneralizedLUT(vector<vector<int>> &coverage, bool x_signed=false, bool y_signed=false);

        bool isIrregular() const override { return true;}
        int getDSPCost() const final {return 0;}
        double getLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) override {return cost;};
        int ownLUTCost(int x_anchor, int y_anchor, int wMultX, int wMultY, bool signedIO) override {return luts;};
        unsigned getArea(void) override {return area;}
        int getRelativeResultLSBWeight(Parametrization const& param) const override {return lsb;};
        int getRelativeResultMSBWeight(Parametrization const& param) const override {return msb;};
        //int getRelativeResultMSBWeight(Parametrization const& param, bool isSignedX, bool isSignedY) const override {return BaseMultiplierXilinxGeneralizedLUT::getRelativeResultMSBWeight(
        //            static_cast<TILE_SHAPE>(param.getShapePara()), isSignedX, isSignedY);};
        bool shapeValid(int x, int y) override;
        bool shapeValid(Parametrization const& param, unsigned x, unsigned y) const override;
        bool signSupX() override {return false;}
        bool signSupY() override {return false;}

        Operator *generateOperator(Operator *parentOp, Target *target, Parametrization const & params) const final;

        /** Factory method */
        static OperatorPtr parseArguments(OperatorPtr parentOp, Target *target , vector<string> &args, UserInterface& ui);
        /** Register the factory */
        static void registerFactory();

        vector<vector<vector<int>>> getEqs() const override {return equations;}
        pair<vector<int>,vector<int>> getDepList() const override {return xy_dependency_list;}
        vector<pair<int,int>> getCombiEqs() const override {return combined_eqs;}
        vector<vector<int>> getCoverage() const override  {return coverage;}
        unsigned getUsedOutputBits() const override {return used_out_bits;}

        static vector<int> get_used_vars(const vector<vector<int>> &eq1, const vector<vector<int>> &eq2);

    private:
        vector<vector<int>> coverage;
        int wX, wY, wR, area, msb, lsb, luts, used_out_bits;
        double cost, efficiency;
        pair<vector<int>,vector<int>> xy_dependency_list;
        vector<vector<vector<int>>> equations;
        vector<pair<int,int>> combined_eqs;

        //static bool shapeValid(int x, int y);
        static bool is_rectangular(vector<vector<int>> &coverage);
        void calc_area(vector<vector<int>> &coverage);
        void calc_tile_properties(bool x_signed, bool y_signed);
        void print_eq(int nx, int ny, vector<vector<int>> &eq, pair<vector<int>,vector<int>> &xy_dep_list);
        void quine_mccluskey(vector<int> &minterms, int input_size, vector<vector<int>> &simplified);
        int count_eq_dependencies(int nx, int ny, vector<vector<int>> &eq, pair<vector<int>,vector<int>> &xy_dep_list);
        void draw_tile(vector<vector<int>> &coverage);
        void count_required_LUT(vector<vector<vector<int>>> &eqs, vector<int> n_dependencys);


    };

    class BaseMultiplierXilinxGeneralizedLUTOp : public Operator {
    public:
        BaseMultiplierXilinxGeneralizedLUTOp(Operator *parentOp, Target* target, bool isSignedX, bool isSignedY, const vector<vector<int>> &coverage, const pair<vector<int>,vector<int>> &xy_dependency_list, const vector<vector<vector<int>>> &equations, const vector<pair<int,int>> &combined_eqs, const unsigned usedOutBits);

        void emulate(TestCase* tc);
        static TestList unitTest(int index);

    private:
        int wX, wY, wR ;       //formal word sizes, actual word sizes
        bool isSignedX, isSignedY;
        const vector<vector<int>> &coverage;
        static const unsigned short bit_pattern[8];
        mpz_class function(int xy);
    };

}
