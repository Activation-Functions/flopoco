#include "flopoco/IntMult/TilingStrategyOptimalILP.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/IntMult/BaseMultiplierLUT.hpp"
#include "flopoco/IntMult/MultiplierTileCollection.hpp"

using namespace std;
namespace flopoco {

TilingStrategyOptimalILP::TilingStrategyOptimalILP(
		unsigned int wX_,
		unsigned int wY_,
		unsigned int wOut_,
		bool signedIO_,
		BaseMultiplierCollection* bmc,
		base_multiplier_id_t prefered_multiplier,
		float occupation_threshold,
		int maxPrefMult,
        MultiplierTileCollection mtc_,
        unsigned guardBits,
        unsigned keepBits,
        mpz_class errorBudget,
        mpz_class &centerErrConstant,
        bool performOptimalTruncation,
        bool squarer):TilingStrategy(
			wX_,
			wY_,
			wOut_,
			signedIO_,
			bmc),
		occupation_threshold_{occupation_threshold},
		max_pref_mult_ {maxPrefMult},
		tiles{mtc_.MultTileCollection},
        guardBits{guardBits},
        keepBits{keepBits},
        eBudget{errorBudget},
        centerErrConstant{centerErrConstant},
        performOptimalTruncation{performOptimalTruncation},
        squarer{squarer}
	{
	    cout << errorBudget << endl;
        mpz_class max64;
        unsigned long long max64u = (1ULL << 52)-1ULL;//UINT64_MAX; //Limit to dynamic of double type
        mpz_import(max64.get_mpz_t(), 1, -1, sizeof max64u, 0, 0, &max64u);
        if(errorBudget <= max64){
            mpz_export(&this->errorBudget, 0, -1, sizeof this->errorBudget, 0, 0, errorBudget.get_mpz_t());
        } else {
            if(performOptimalTruncation)
                cout << "WARNING: errorBudget or constant exceeds the number range of uint64, switching to optiTrunc=0" << endl;
            this->errorBudget = 0;
            this->performOptimalTruncation = false;
        }
        cout << this->errorBudget << endl;

	    cout << "guardBits " << guardBits << " keepBits " << keepBits << endl;
        for(auto &p:tiles)
        {
                cout << p->getLUTCost(0, 0, wX, wY, false) << " " << p->getType() << endl;
        }
	}

void TilingStrategyOptimalILP::solve()
{

#ifndef HAVE_SCALP
    throw "Error, TilingStrategyOptimalILP::solve() was called but FloPoCo was not built with ScaLP library";
#else
    solver = new ScaLP::Solver(ScaLP::newSolverDynamic({target->getILPSolver(),"Gurobi","CPLEX","SCIP","LPSolve"}));
    cout << "using ILP solver " << solver->getBackendName() << " (whish was " << target->getILPSolver() << ")" << endl;

    if(solver->getBackendName().find("LPSolve") != std::string::npos)
    {
      cout << "LPSolve is used, disabling presolve as this caused problems in the past" << endl;
      solver->presolve = false;
    }
    solver->timeout = target->getILPTimeout();

    long long optTruncNumericErr = 0;
    do {                                //Loop as a workaround for numeric solver problems during optimal truncation to ensure that the truncation error margin is met
        solver->reset();
        TilingStrategy::solution.clear();
        constructProblem();

        // Try to solve
        cout << "starting solver, this might take a while..." << endl;
        solver->quiet = false;
        ScaLP::status stat = solver->solve();

        // print results
        cerr << "The result is " << stat << endl;
        //cerr << solver->getResult() << endl;
        ofstream result_file;
        result_file.open("result.txt");
        result_file << solver->getResult();
        result_file.close();
        ScaLP::Result res = solver->getResult();

        //parse solution
        cout << "centerErrConstant was: " << centerErrConstant << endl;
        unsigned long long new_constant = 0;
        double total_cost = 0, sum1 = 0, sum2 = 0, sum3 = 0, sum4 = 0;
        int dsp_cost = 0, own_lut_cost = 0;
        for (auto &p:res.values) {
            if (p.second >= 0.5) {     //parametrize all multipliers at a certain position, for which the solver returned 1 as solution, to flopoco solution structure
                std::string var_name = p.first->getName();
                if (var_name.substr(1, 1) == "d") {
                    int mult_id = stoi(var_name.substr(2, dpS));
                    int x_negative = (var_name.substr(2 + dpS, 1).compare("m") == 0) ? 1 : 0;
                    int m_x_pos = stoi(var_name.substr(2 + dpS + x_negative, dpX)) * ((x_negative) ? (-1) : 1);
                    int y_negative = (var_name.substr(2 + dpS + x_negative + dpX, 1).compare("m") == 0) ? 1 : 0;
                    int m_y_pos = stoi(var_name.substr(2 + dpS + dpX + x_negative + y_negative, dpY)) *
                                  ((y_negative) ? (-1) : 1);
                    cout << "is true:  " << setfill(' ') << setw(dpY) << mult_id << " " << setfill(' ') << setw(dpY)
                         << m_x_pos << " " << setfill(' ') << setw(dpY) << m_y_pos << " cost: " << setfill(' ')
                         << setw(5) << tiles[mult_id]->getLUTCost(m_x_pos, m_y_pos, wX, wY, signedIO) << std::endl;

                    total_cost += (double) tiles[mult_id]->getLUTCost(m_x_pos, m_y_pos, wX, wY, signedIO);
                    own_lut_cost += tiles[mult_id]->ownLUTCost(m_x_pos, m_y_pos, wX, wY, signedIO);
                    dsp_cost += (double) tiles[mult_id]->getDSPCost();
                    auto coord = make_pair(m_x_pos, m_y_pos);
                    solution.push_back(make_pair(
                            tiles[mult_id]->getParametrisation().tryDSPExpand(m_x_pos, m_y_pos, wX, wY, signedIO),
                            coord));
                }
                if (var_name.substr(0, 1) == "c") {
                    int c_id = stoi(var_name.substr(1, dpC));
                    new_constant |= (1ULL << c_id);
                    cout << var_name << " pos " << c_id << " dpC " << dpC << endl;
                }
            }
            //check variables for numeric derivations due to rounding for optimal truncation.
            if (p.first->getName().substr(1, 1) == "b") {
                int x_negative = (p.first->getName().substr(2, 1).compare("m") == 0) ? 1 : 0;
                int m_x_pos = stoi(p.first->getName().substr(2 + x_negative, dpX)) * ((x_negative) ? (-1) : 1);
                int y_negative = (p.first->getName().substr(2 + x_negative + dpX, 1).compare("m") == 0) ? 1 : 0;
                int m_y_pos = stoi(p.first->getName().substr(2 + dpX + x_negative + y_negative, dpY)) *
                              ((y_negative) ? (-1) : 1);
                sum1 += p.second * (1LL << (m_x_pos + m_y_pos));
                if (p.second >= 0.5) sum2 += (1LL << (m_x_pos + m_y_pos));
            }
            if (p.first->getName().substr(0, 1) == "c") {
                int shift = stoi(p.first->getName().substr(1, dpC));
                sum3 += p.second * (1 << shift);
                if (p.second >= 0.5) sum4 += (1 << shift);
            }
        }
        cout << "Total LUT cost:" << total_cost << std::endl;
        cout << "Own LUT cost:" << own_lut_cost << std::endl;
        cout << "Total DSP cost:" << dsp_cost << std::endl;
        cout << std::setprecision(20) << sum1 << " s2 " << std::setprecision(20) << sum2 << " c:"
             << std::setprecision(20) << sum3 << " s2 " << std::setprecision(20) << sum4 << endl;

        if(performOptimalTruncation){
            //refresh centerErrConstant according to ILP solution
            //centerErrConstant = new_constant;
            mpz_import(centerErrConstant.get_mpz_t(), 1, -1, sizeof new_constant, 0, 0, &new_constant);
            cout << "centerErrConstant now is: " << centerErrConstant << endl;

            //check for numeric errors in solution
            optTruncNumericErr = (long long)ceil(fabs((sum1 + sum3) - (sum2 + sum4)));
            if(0 < optTruncNumericErr){
                cout << "Warning: Numeric problems in solution, repeating ILP with Offset for Error of " << optTruncNumericErr << endl;
                errorBudget -= optTruncNumericErr;      //Reduce errorBudget by scope of numeric derivation for next iteration
            }
        }

    } while(performOptimalTruncation && eBudget+centerErrConstant < IntMultiplier::checkTruncationError(solution, guardBits, eBudget, centerErrConstant, wX, wY, signedIO) && 0 < optTruncNumericErr);
/*
    solution.push_back(make_pair(tiles[1]->getParametrisation().tryDSPExpand(0, 0, wX, wY, signedIO), make_pair(0, 0)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(16, 0, wX, wY, signedIO), make_pair(16, 0)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(32, 0, wX, wY, signedIO), make_pair(32, 0)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(0, 24, wX, wY, signedIO), make_pair(0, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(16, 24, wX, wY, signedIO), make_pair(16, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(32, 24, wX, wY, signedIO), make_pair(32, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(48, 24, wX, wY, signedIO), make_pair(48, 24)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(16, 48, wX, wY, signedIO), make_pair(16, 48)));
    solution.push_back(make_pair(tiles[0]->getParametrisation().tryDSPExpand(32, 48, wX, wY, signedIO), make_pair(32, 48)));
*/
//    solution.push_back(make_pair(tiles[1]->getParametrisation(), make_pair(0, 0)));
/*    solution.push_back(make_pair(tiles[9]->getParametrisation().tryDSPExpand(0, 0, wX, wY, signedIO), make_pair(0, 0)));
    solution.push_back(make_pair(tiles[5]->getParametrisation().tryDSPExpand(5, 0, wX, wY, signedIO), make_pair(5, 0)));*/
#endif
}

#ifdef HAVE_SCALP
void TilingStrategyOptimalILP::constructProblem()
{
    cout << "constructing problem formulation..." << endl;
    wS = tiles.size();

    for (auto const& i : tiles) {
        std::cout << i->getType() << " weight=" << i->getParametrisation().getTilingWeight()  << endl;
    }

    //Assemble cost function, declare problem variables
    cout << "   assembling cost function, declaring problem variables..." << endl;
    ScaLP::Term obj;
    prodWidth = IntMultiplier::prodsize(wX, wY, signedIO, signedIO);
    int x_neg = 0, y_neg = 0;
    for(int s = 0; s < wS; s++){
        x_neg = (x_neg < (int)tiles[s]->wX())?tiles[s]->wX() - 1:x_neg;
        y_neg = (y_neg < (int)tiles[s]->wY())?tiles[s]->wY() - 1:y_neg;
    }
    int nx = wX-1, ny = wY-1, ns = wS-1, nc = prodWidth-wOut; dpX = 1; dpY = 1; dpS = 1; dpC = 1;   //calc number of decimal places, for var names
    nx = (x_neg > nx)?x_neg:nx;                                                                     //in case the extend in negative direction is larger
    ny = (y_neg > ny)?y_neg:ny;
    while (nx /= 10)
        dpX++;
    while (ny /= 10)
        dpY++;
    while (ns /= 10)
        dpS++;
    while (nc /= 10)
        dpC++;

    vector<ScaLP::Term> bitsinColumn(prodWidth + 1), constVecBits(prodWidth + 5);
    vector<vector<vector<ScaLP::Variable>>> solve_Vars(wS, vector<vector<ScaLP::Variable>>(wX+x_neg, vector<ScaLP::Variable>(wY+y_neg)));
    ScaLP::Term maxEpsTerm, minEpsTerm;
    // add the Constraints
    cout << "   adding the constraints to problem formulation..." << endl;
    for(int y = 0; y < wY; y++){
        for(int x = 0; x < wX; x++){
            if(squarer && x < y) continue;
            stringstream consName;
            consName << "p" << setfill('0') << setw(dpX) << x << setfill('0') << setw(dpY) << y;            //one constraint for every position in the area to be tiled
            ScaLP::Term pxyTerm;
            for(int s = 0; s < wS; s++){					//for every available tile...
                int diff = (squarer)?std::abs(int(tiles[s]->wX()-tiles[s]->wY())):0;
                for(int ys = 0 - tiles[s]->wY() + 1; ys <= y + diff; ys++){					//...check if the position x,y gets covered by tile s located at position (xs, ys) = (x-wtile..x, y-htile..y)
                    for(int xs = 0 - tiles[s]->wX() + 1; xs <= x + diff; xs++){
                        if(occupation_threshold_ == 1.0 && ((wX - xs) < (int)tiles[s]->wX() || (wY - ys) < (int)tiles[s]->wY())) break;
                        if(squarer && tiles[s]->isSquarer() && xs != ys) continue;                 //squarers should only be placed in the diagonal
                        if(squarer && xs < ys) continue;                 //within squarers tiles should not be placed below the diagonal
                        if(tiles[s]->shape_contribution(x, y, xs, ys, wX, wY, signedIO, squarer)){
                            if((wOut < (int)prodWidth) && ((xs+tiles[s]->wX()+ys+tiles[s]->wY()-2) < ((int)prodWidth-wOut-guardBits))) break;
                            if(signedIO && (wX < xs+(int)tiles[s]->wX() || wY < ys+(int)tiles[s]->wY()) ) break;                      //Avoid considering protruding cases in signed case, tiling is valid without, but it would require compensation in c.tree
                            if(signedIO && ((wX == xs+(int)tiles[s]->wX() && !tiles[s]->signSupX()) || (wY == ys+(int)tiles[s]->wY() && !tiles[s]->signSupY()) )) break; //Avoid placing tiles without signed support at the bottom and left |_ edge of the area to be tiled.
                            if(tiles[s]->shape_utilisation(xs, ys, wX, wY, signedIO) >=  occupation_threshold_ ){
                                if(solve_Vars[s][xs+x_neg][ys+y_neg] == nullptr){
                                    stringstream nvarName;
                                    nvarName << " d" << setfill('0') << setw(dpS) << s << ((xs < 0)?"m":"") << setfill('0') << setw(dpX) << ((xs<0)?-xs:xs) << ((ys < 0)?"m":"")<< setfill('0') << setw(dpY) << ((ys<0)?-ys:ys) ;
                                    //std::cout << nvarName.str() << endl;
                                    ScaLP::Variable tempV = ScaLP::newBinaryVariable(nvarName.str());
                                    solve_Vars[s][xs+x_neg][ys+y_neg] = tempV;
                                    obj.add(tempV, (double)tiles[s]->getLUTCost(xs, ys, wX, wY, signedIO));    //append variable to cost function
                                }

                                if(!squarer || tiles[s]->shape_contribution(x, y, xs, ys, wX, wY, signedIO, false))
                                    pxyTerm.add(solve_Vars[s][xs+x_neg][ys+y_neg], tiles[s]->getParametrisation().getTilingWeight());          //add decision variable to eq for position (x,y) when the respective tile s at (xs,ys) covers this position
                                if((squarer && !tiles[s]->isSquarer() && xs <= ys+(int)tiles[s]->wY()-1 && tiles[s]->shapeValid(y-xs,x-ys) && x != y) || (tiles[s]->isSquarer() && x != y)){   //consideration of symmetries for squarers
                                    pxyTerm.add(solve_Vars[s][xs+x_neg][ys+y_neg], tiles[s]->getParametrisation().getTilingWeight());          //in squarers the symmetric position for the current eq. below the diagonal is covered by the tile s
                                }

                                if(squarer && tiles[s]->getParametrisation().getTilingWeight()<0){      //Handling of the sign extension bits in dynamically calculated constant bit vector
                                    if(tiles[s]->wX() == 1 && tiles[s]->wY() == 1 && xs == wX-1 && ys == wY-1) break; //the 1x1 tile with (1,1) signedness should not get a sign extension vector.
                                    for(unsigned i = xs+ys+tiles[s]->getRelativeResultMSBWeight(tiles[s]->getParametrisation(),signedIO && xs+(int)tiles[s]->wX() == wX, signedIO && ys+(int)tiles[s]->wY() == wY); i < prodWidth; i++){
                                        constVecBits[i].add(solve_Vars[s][xs+x_neg][ys+y_neg], 1);
                                    }
                                }

                            }
                        }
                    }
                }
            }
            ScaLP::Constraint c1Constraint;
            if(performOptimalTruncation && (wOut < (int)prodWidth) && ((x + y) < ((int)prodWidth - wOut))) {
                stringstream nvarName;
                nvarName << " b" << ((x < 0) ? "m" : "") << setfill('0') << setw(dpX) << ((x < 0) ? -x : x)
                         << ((y < 0) ? "m" : "") << setfill('0') << setw(dpY) << ((y < 0) ? -y : y);
                ScaLP::Variable tempV = ScaLP::newBinaryVariable(nvarName.str());
                if(signedIO && (wX-1 == x || wY-1 == y)){
                    minEpsTerm.add(tempV, (+1LL) * (1LL << (x + y)));
                    minEpsTerm.add((-1LL) << (x + y));
                    if(squarer && x != y){                      //with a squarer the tiling is mirrored at the diagonal, so missing tiles on top also cause an error below
                        minEpsTerm.add(tempV, (+1LL) * (1LL << (x + y)));
                        minEpsTerm.add((-1LL) << (x + y));
                    }
                } else {
                    maxEpsTerm.add(tempV, (-1LL) * (1LL << (x + y)));
                    maxEpsTerm.add(1ULL << (x + y));
                    if(squarer && x != y){                      //with a squarer the tiling is mirrored at the diagonal, so missing tiles on top also cause an error below
                        maxEpsTerm.add(tempV, (-1LL) * (1LL << (x + y)));
                        maxEpsTerm.add(1ULL << (x + y));
                    }
                }
                c1Constraint = pxyTerm - ((squarer && x != y)?2:1) * tempV == 0;
            } else if(!performOptimalTruncation && (wOut < (int)prodWidth) && ((x + y) < (int)(prodWidth - wOut - guardBits))){
                //c1Constraint = pxyTerm <= (bool)1;
            } else if(!performOptimalTruncation && (wOut < (int)prodWidth) && ((x + y) == (int)(prodWidth - wOut - guardBits))){
                //cout << "keepBit to place: " << keepBits << endl;
                if((keepBits)?keepBits--:0){
                    c1Constraint = pxyTerm == ((squarer && x != y)?2:1);
                    if(squarer && x != y && keepBits) keepBits--;           //consider symmetric bit in squarer
                    //cout << "keepBit at" << x << "," << y << endl;
                } else {
                    //cout << "NO keepBit at" << x << "," << y << endl;
                }
            } else {
                c1Constraint = pxyTerm == ((!squarer || x == y)?1.0:2.0);
            }

            c1Constraint.name = consName.str();
            solver->addConstraint(c1Constraint);
        }
    }

    //limit use of DSPs
    if(0 <= max_pref_mult_) {
        //check if DSP tiles are available
        bool nDSPTiles = false;
        for (int s = 0; s < wS; s++) {
            if (tiles[s]->getDSPCost()) {
                nDSPTiles = true;
                break;
            }
        }

        if (nDSPTiles) {
            cout << "   adding the constraint to limit the use of DSP-Blocks to " << max_pref_mult_ << " instances..."
                 << endl;
            stringstream consName;
            consName << "limDSP";
            ScaLP::Term pxyTerm;
            for (int y = 0 - y_neg; y < wY; y++) {
                for (int x = 0 - x_neg; x < wX; x++) {
                    for (int s = 0; s < wS; s++)
                        if (solve_Vars[s][x + x_neg][y + y_neg] != nullptr)
                            for (int c = 0; c < tiles[s]->getDSPCost(); c++)
                                pxyTerm.add(solve_Vars[s][x + x_neg][y + y_neg], 1);
                }
            }
            ScaLP::Constraint c1Constraint = pxyTerm <= max_pref_mult_;     //set max usage equ.
            c1Constraint.name = consName.str();
            solver->addConstraint(c1Constraint);
        }
    }

    //make shure the available precision is present in case of truncation
    if(performOptimalTruncation == true && (wOut < (int)prodWidth))
    {
        cout << "   multiplier is truncated by " << (int)prodWidth-wOut << " bits (err=" << (unsigned long)wX*(((unsigned long)1<<((int)wOut-guardBits))) << "), ensure sufficient precision..." << endl;
        cout << "   guardBits=" << guardBits << endl;
        cout << "   g=" << guardBits << " k=" << keepBits << " errorBudget=" << errorBudget << " difference to conservative est: " << errorBudget-(long long)(((unsigned long)1)<<((prodWidth-(int)wOut-1)-1)) << endl;

        stringstream nvarName;
        nvarName << "C";
        ScaLP::Variable Cvar = ScaLP::newIntegerVariable(nvarName.str());

        // C = 2^i*c_i + 2^(i+1)*c_(i+1)...
        ScaLP::Term cTerm;
        vector<ScaLP::Variable> cVars(guardBits-1);
        for(unsigned i = 0; i < guardBits-1; i++){
            stringstream nvarName;
            nvarName << "c" << prodWidth-wOut-guardBits+i;
            cout << nvarName.str() << endl;
            cVars[i] = ScaLP::newBinaryVariable(nvarName.str());
            cTerm.add(cVars[i],  ( 1ULL << (prodWidth-wOut-guardBits+i)));
            obj.add(cVars[i], (double)0.65);    //append variable to cost function
        }
        ScaLP::Constraint cConstraint = cTerm - Cvar == 0;
        stringstream cName;
        cName << "cCons";
        cConstraint.name = cName.str();
        solver->addConstraint(cConstraint);

        //Limit value of constant to center the error around 0 to the error budget
        ScaLP::Constraint cLimConstraint = Cvar < errorBudget;
        stringstream cLimName;
        cLimName << "cLimCons";
        cLimConstraint.name = cLimName.str();
        solver->addConstraint(cLimConstraint);

        //Limit the error budget
        cout << "  maxErr=" << errorBudget << endl;
        ScaLP::Constraint maxErrConstraint = maxEpsTerm - Cvar < errorBudget;
        stringstream maxErrName;
        maxErrName << "maxEps";
        maxErrConstraint.name = maxErrName.str();
        solver->addConstraint(maxErrConstraint);

        //Limit the error budget
        cout << "  minErr=" << errorBudget << endl;
        ScaLP::Constraint minErrConstraint = -minEpsTerm + Cvar < errorBudget;
        stringstream minErrName;
        minErrName << "minEps";
        minErrConstraint.name = minErrName.str();
        solver->addConstraint(minErrConstraint);
    }

    //consideration of the compression cost of the sign extension vector bits
    if(squarer){
        vector<ScaLP::Variable> cvBits(prodWidth+5);
        vector<ScaLP::Variable> ovVars(prodWidth+5);
        for(unsigned i = 0; i < prodWidth+5; i++){
            stringstream cvarName;
            cvarName << "v" << setfill('0') << setw(dpC) << i;
            //cout << cvarName.str() << " weight " << (double)(1ULL << i) << endl;
            cvBits[i] = ScaLP::newBinaryVariable(cvarName.str());
            obj.add(cvBits[i], 0.65);    //append variable to cost function

            stringstream ovarName;
            ovarName << "o" << setfill('0') << setw(dpC) << i;
            //cout << cvarName.str() << " weight " << (double)(1ULL << i) << endl;
            ovVars[i] = ScaLP::newIntegerVariable(ovarName.str());
            constVecBits[i].add(cvBits[i], -1);                 //constant bit in col i
            constVecBits[i].add(ovVars[i], -2);                 //for carry to next constraint
            if(0 < i) constVecBits[i].add(ovVars[i-1], 1);   //carry from previous constraint
            //Calculate individual constant vector bits
            ScaLP::Constraint cvbConstraint = constVecBits[i] == 0;
            stringstream consName;
            consName << "cBit" << setfill('0') << setw(dpC) << i;
            cvbConstraint.name = consName.str();
            solver->addConstraint(cvbConstraint);
        }
    }

    // Set the Objective
    cout << "   setting objective (minimize cost function)..." << endl;
    solver->setObjective(ScaLP::minimize(obj));

    // Write Linear Program to file for debug purposes
    cout << "   writing LP-file for debuging..." << endl;
    solver->writeLP("tile.lp");
}


#endif

}   //end namespace flopoco
