#include "flopoco/IntMult/BaseMultiplierXilinxGeneralizedLUT.hpp"
#include "flopoco/Tables/TableOperator.hpp"
#include "flopoco/IntMult/IntMultiplier.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT6.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_CARRY4.hpp"
#include "flopoco/PrimitiveComponents/Xilinx/Xilinx_LUT_compute.hpp"

namespace flopoco
{

  BaseMultiplierXilinxGeneralizedLUT::BaseMultiplierXilinxGeneralizedLUT(
    vector<vector<int>> &coverage, bool x_signed, bool y_signed
  ) : BaseMultiplierCategory(
    (int) coverage.size(),
    (int) coverage[0].size(),
    x_signed,
    y_signed,
    -1,
    "BaseMultiplierXilinxGeneralizedLUT_" + to_string(coverage.size()) + "x" + to_string(coverage[0].size()),
    is_rectangular(coverage)
  ), coverage(coverage), wX(coverage.size()), wY(coverage[0].size())
  {
    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) draw_tile(coverage);
    calc_area(coverage);
    calc_tile_properties(x_signed, y_signed);

  }

  bool BaseMultiplierXilinxGeneralizedLUT::is_rectangular(vector<vector<int>> &coverage)
  {
    for (int y = 0; y < coverage[0].size(); y++)
    {
      for (int x = 0; x < coverage.size(); x++)
      {
        if (!coverage[x][y]) return false;
      }
    }
    return true;
  }

  void BaseMultiplierXilinxGeneralizedLUT::calc_area(vector<vector<int>> &coverage)
  {
    area = 0;
    for (int y = 0; y < coverage[0].size(); y++)
    {
      for (int x = 0; x < coverage.size(); x++)
      {
        if (coverage[x][y]) area++;
      }
    }
  }

  void BaseMultiplierXilinxGeneralizedLUT::calc_tile_properties(bool x_signed, bool y_signed)
  {
    //generate list of inputs the partial products depend on
    pair<vector<int>, vector<int>> xy_dep_list;
    for (int y = 0; y < coverage[0].size(); y++)
    {
      for (int x = 0; x < coverage.size(); x++)
      {
        if (coverage[x][y])
        {
          bool found_x = false, found_y = false;
          for (int cx = 0; cx < xy_dep_list.first.size(); cx++)
          {
            if (xy_dep_list.first[cx] == x) found_x = true;
          }
          if (!found_x) xy_dep_list.first.push_back(x);
          for (int cy = 0; cy < xy_dep_list.second.size(); cy++)
          {
            if (xy_dep_list.second[cy] == y) found_y = true;
          }
          if (!found_y) xy_dep_list.second.push_back(y);
        }
      }
    }

    vector<int> tile_trueth_table;

    long long used_bits = 0, pp_min = 2147483647, pp_max = -2147483647;
    int n_max = 0, p_max = 0;
    for (unsigned long long y = 0; y < (1ULL << xy_dep_list.second.size()); y++)
    {
      for (unsigned long long x = 0; x < (1ULL << xy_dep_list.first.size()); x++)
      {
        long long sum = 0;
        for (int yi = 0; yi < xy_dep_list.second.size(); yi++)
        {
          for (int xi = 0; xi < xy_dep_list.first.size(); xi++)
          {
            sum += coverage[xy_dep_list.first[xi]][xy_dep_list.second[yi]] * ((((x & (1ULL << xi)) ? 1 : 0) * (1ULL << xy_dep_list.first[xi])) * (((y & (1ULL << yi)) ? 1 : 0) * (1ULL << xy_dep_list.second[yi]))) *
                   ((x_signed && (xy_dep_list.first[xi] == coverage.size() - 1)) ? -1 : 1) * ((y_signed && (xy_dep_list.second[yi] == coverage[0].size() - 1)) ? -1 : 1);
          }
        }
        if (sum < 0)
        {
          unsigned long long n = 0;
          for (; (1 << n) <= ~sum; n++)
          {
            used_bits |= (sum & (1 << n));
          }
          used_bits |= (sum & (1 << n));
          n_max = ((n_max < n) ? n : n_max);
        }
        else
        {
          for (unsigned long long n = 0; (1 << n) <= sum; n++)
          {
            used_bits |= (sum & (1 << n));
            p_max = ((p_max < n) ? n : p_max);
          }
        }
        pp_min = ((sum < pp_min) ? sum : pp_min);
        pp_max = ((pp_max < sum) ? sum : pp_max);
        //cerr << y << "," << x << " sum=" << sum << endl;
        tile_trueth_table.push_back(sum);
      }
    }
    //cerr << endl;
    if ((x_signed || y_signed) && (n_max == p_max)) used_bits |= (1 << (n_max + 1));
    //cerr << "used bits=" << used_bits << endl;

    lsb = 0;
    msb = 0;
    vector<int> out_weights;
    unsigned long long temp_max = used_bits, temp_lsb = used_bits;
    while ((temp_lsb & (1 << lsb)) == 0) lsb++;
    while (temp_max >>= 1) msb++;
    wR = 0;
    for (unsigned long long n = 0; n <= msb; n++)
    {
      if (used_bits & (1 << n))
      {
        wR++;
        out_weights.push_back((1 << n));
        //cerr << out_weights.back() << ";";
      }
      //cerr << ((used_bits & (1<<n))?1:0) << ",";
    }
    REPORT(LogLevel::VERBOSE, endl);
    REPORT(LogLevel::VERBOSE, "r={" << pp_min << ".." << pp_max << "}" );
    REPORT(LogLevel::VERBOSE, "lsb=" << lsb << " msb=" << msb << " pp_out=" << wR );

    vector<vector<int>> minterms(wR);
    int input_size = xy_dep_list.first.size() + xy_dep_list.second.size();
    for (unsigned long long y = 0; y < (1ULL << xy_dep_list.second.size()); y++)
    {
      for (unsigned long long x = 0; x < (1ULL << xy_dep_list.first.size()); x++)
      {
        int minterm = (y << xy_dep_list.first.size()) | x, outp_idx = 0;
        for (int i = 0; i <= msb; i++)
        {
          if (used_bits & (1ULL << i))
          {
            if (tile_trueth_table[minterm] & (1ULL << i)) minterms[outp_idx].push_back(minterm);
            outp_idx++;
          }
        }
      }
    }

    vector<vector<vector<int>>> simplified(wR);
    vector<int> n_dependencys(wR);
    for (int i = 0; i < minterms.size(); i++)
    {
      //cerr << "the eq. for out bit n=" << i << " has " << minterms[i].size() << "terms" << endl;
      //cerr << "r" << i << "=";
      quine_mccluskey(minterms[i], input_size, simplified[i]);
      n_dependencys[i] = count_eq_dependencies(xy_dep_list.first.size(), xy_dep_list.second.size(), simplified[i], xy_dep_list);
      if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) {
	      cerr << "r" << i << "(" << n_dependencys[i] << ")=";
	      print_eq(xy_dep_list.first.size(), xy_dep_list.second.size(), simplified[i], xy_dep_list);
	      cerr << endl;
      }
    }
    count_required_LUT(simplified, n_dependencys);
    xy_dependency_list = xy_dep_list;
    equations = simplified;
    used_out_bits = used_bits;

    //set weights of output(s)
    int w = 0;
    vector<int> weights, out_sizes;
    while ((1 << w) <= used_bits)
    {
      while ((used_bits & (1 << w)) == 0) w++;
      weights.push_back(w);
      while (used_bits & (1 << w))
      {
        if (out_sizes.size() < weights.size()) out_sizes.push_back(0);
        out_sizes.back()++;
        w++;
      }
    }
    this->tile_param.setOutputWeights(weights);
    this->tile_param.setOutputSizes(out_sizes);
    //cerr << "the tile has " << weights.size() << " outputs" << endl;
  }


  void BaseMultiplierXilinxGeneralizedLUT::quine_mccluskey(vector<int> &minterms, int input_size, vector<vector<int>> &simplified)
  {
    vector<vector<vector<tuple<int, int, vector<int>, bool>>>> level_terms;      //simplification step, variables set to one class, minterm within the class, pair with minterm and origin index
    vector<vector<tuple<int, int, vector<int>, bool>>> terms(input_size + 1);
    for (int i = 0; i < minterms.size(); i++)
    {
      int used_inputs = 0;
      for (int j = 0; j < input_size; j++)
      {
        if (minterms[i] & (1 << j)) used_inputs++;
      }
      //cerr << "term " << minterms[i] << " no=" << i << " has " << used_inputs << " inputs set to 1." << endl;
      tuple<int, int, vector<int>, bool> temp = {minterms[i], 0, {i}, false};   //{bit pattern, diff, source list}
      terms[used_inputs].push_back(temp);
    }
    level_terms.push_back(terms);

    int step_idx = 0;
    bool simplification_possible;
    do
    {
      simplification_possible = false;
      vector<vector<tuple<int, int, vector<int>, bool>>> terms_comb(input_size + 1);
      for (int i = 0; i < level_terms[step_idx].size() - 1; i++)
      {               //select class of inputs set to 1
        for (int j = 0; j < level_terms[step_idx][i].size(); j++)
        {          //select minterm within the current class
          for (int k = 0; k < level_terms[step_idx][i + 1].size(); k++)
          {    //select minterm within the subsequent class
            int diff = (get<0>(level_terms[step_idx][i][j]) ^ get<0>(level_terms[step_idx][i + 1][k])) & ~((step_idx == 0) ? 0 : get<1>(level_terms[step_idx][i][j])), temp = (diff << 1), diffs = 0;
            while (temp) if ((temp >>= 1) & 1) diffs++;                   //count minterms the differ only by one additional bit set from current to next class
            //cerr << "class i=" << i << " j=" << j << " k=" << k << " " << get<0>(level_terms[step_idx][i][j]) << "," << get<0>(level_terms[step_idx][i+1][k]) << " diff=" << (get<0>(level_terms[step_idx][i][j]) ^ get<0>(level_terms[step_idx][i+1][k])) << " diffs=" << diffs << endl;
            if (diffs == 1 && (get<1>(level_terms[step_idx][i][j]) == get<1>(level_terms[step_idx][i + 1][k])))
            {
              //cerr << "found terms " << (get<0>(level_terms[step_idx][i][j])) << " and " << (get<0>(level_terms[step_idx][i+1][k])) << " that differ only by one bit " << diff << " in current=" << i << " and subsequent=" << i+1 << " class with the same ignore mask=" << (get<1>(level_terms[step_idx][i][j])) << endl;
              vector<int> origin_list;
              int n;
              get<3>(level_terms[step_idx][i][j]) = true;     //mark origin term as covered
              get<3>(level_terms[step_idx][i + 1][k]) = true;
              for (n = 0; n < get<2>(level_terms[step_idx][i][j]).size(); n++) origin_list.push_back(get<2>(level_terms[step_idx][i][j])[n]);
              for (n = 0; n < get<2>(level_terms[step_idx][i + 1][k]).size(); n++) origin_list.push_back(get<2>(level_terms[step_idx][i + 1][k])[n]);
              for (n = 0; n < terms_comb[i].size(); n++)
                if (((get<0>(terms_comb[i][n]) & ~get<1>(terms_comb[i][n])) == (get<0>(level_terms[step_idx][i][j]) & ~diff)) && (get<1>(terms_comb[i][n]) == (diff | get<1>(level_terms[step_idx][i][j]))))
                { break; }   //do not push duplicate terms to list of merged minterms of current simplification step
              if (n == terms_comb[i].size())
              {  //term was not already present
                terms_comb[i].push_back({get<0>(level_terms[step_idx][i][j]), diff | get<1>(level_terms[step_idx][i][j]), origin_list, false});  //push to list of current step
              }
              simplification_possible = true;
            }
          }
        }
      }
      if (simplification_possible)
      {
        level_terms.push_back(terms_comb);
        step_idx++;
      }
    } while (simplification_possible);


    //create source list
    vector<tuple<int, int, vector<int>>> source_list;             //class, element in class, implicants source list
    for (int s = level_terms.size() - 1; 0 <= s; s--)
    {             //select simplification step
      for (int c = level_terms[s].size() - 1; 0 <= c; c--)
      {         //select class of set bits
        for (int t = level_terms[s][c].size() - 1; 0 <= t; t--)
        {  //select term
          if (get<3>(level_terms[s][c][t]) == false)
          {
            source_list.insert(source_list.begin(), {get<0>(level_terms[s][c][t]), get<1>(level_terms[s][c][t]), get<2>(level_terms[s][c][t])});
          }
        }
      }
    }

    //create second quine table
    std::vector<std::vector<int> > second_quine_table(source_list.size(), std::vector<int>(minterms.size()));
    for (int j = 0; j < source_list.size(); j++)
    {                        //select primeimplicant
      for (int k = 0; k < get<2>(source_list[j]).size(); k++)
      {    //select minterm covered by current primeimplicant
        second_quine_table[j][get<2>(source_list[j])[k]] = 1;
      }
    }

    //search for core primeimplicants
    vector<int> covered_minterms(minterms.size());
    vector<int> core_primeimplicants(source_list.size());
    for (int j = 0; j < second_quine_table.size(); j++)
    {             //select primeimplicant
      bool is_core_implicant = false;
      for (int k = 0; k < second_quine_table[j].size(); k++)
      {      //select minterm
        if (second_quine_table[j][k])
        {
          int pe_cov = 0;
          for (int l = 0; l < second_quine_table.size(); l++)
          { //check for exclusive coverage of minterms by current primeimplicant
            if (second_quine_table[l][k]) pe_cov++;
          }
          if (pe_cov == 1)
          {
            core_primeimplicants[j] = 1;
            for (int l = 0; l < get<2>(source_list[j]).size(); l++)
            {    //select minterm covered by current primeimplicant
              covered_minterms[get<2>(source_list[j])[l]] = 1;       //mark all minterms covered by core prime implicant as covered
            }
          }
        }
      }
    }

    //search for dominant columns
    for (int cur_mt = 0; cur_mt < minterms.size(); cur_mt++)
    {            //select minterm
      //if(covered_minterms[cur_mt]) cerr << "col=" << cur_mt << " is already covered" << endl;
      if (covered_minterms[cur_mt]) continue;
      for (int comp_mt = 0; comp_mt < minterms.size(); comp_mt++)
      {        //minterm to compare to
        if (covered_minterms[comp_mt] || cur_mt == comp_mt) continue;
        int cur_set = 0, ref_set = 0;
        for (int pimp = 0; pimp < second_quine_table.size(); pimp++)
        {            //go through the primeimplicants for both minterm to find which is dominant
          //cerr << "c_col=" << cur_mt << " r_col=" << comp_mt << " row=" << pimp << " cpi=" << core_primeimplicants[pimp] << " cur=" << second_quine_table[pimp][cur_mt] << " ref=" << second_quine_table[pimp][comp_mt] << " skip=" << (!second_quine_table[pimp][cur_mt] && second_quine_table[pimp][comp_mt]) << endl;
          if (core_primeimplicants[pimp]) continue;
          //cerr << "row=" << pimp << " cur=" << second_quine_table[pimp][cur_mt] << " ref=" << second_quine_table[pimp][comp_mt] << " skip=" << (!second_quine_table[pimp][cur_mt] && second_quine_table[pimp][comp_mt]) << endl;
          if (!second_quine_table[pimp][cur_mt] && second_quine_table[pimp][comp_mt])
          {
            cur_set = 0;
            break;
          }   //current minterm does not use p.i. which the other m.t uses so it cant domiante the other m.t column
          if (second_quine_table[pimp][cur_mt] && second_quine_table[pimp][comp_mt])
          {
            cur_set++;
            ref_set++;
          };
          if (second_quine_table[pimp][cur_mt] && !second_quine_table[pimp][comp_mt]) cur_set++;
        }
        if (cur_set != 0 && ref_set <= cur_set) covered_minterms[cur_mt] = 1;                      //mark dominant minterm column as covered
        //if(cur_set != 0 && ref_set<=cur_set) cerr << "column=" << cur_mt << " is dominant over col=" << comp_mt << endl;
      }
    }

    //search for dominant rows
    vector<int> skip_primeimplicants(source_list.size());
    for (int cur_pimp = 0; cur_pimp < second_quine_table.size(); cur_pimp++)
    {            //go through the primeimplicants to find which row is dominant
      if (core_primeimplicants[cur_pimp] || skip_primeimplicants[cur_pimp]) continue;
      for (int ref_pimp = 0; ref_pimp < second_quine_table.size(); ref_pimp++)
      {        //go through the primeimplicants to compare the covered minterm to
        if (core_primeimplicants[ref_pimp] || cur_pimp == ref_pimp) continue;
        int cur_set = 0, ref_set = 0;
        for (int cur_mt = 0; cur_mt < minterms.size(); cur_mt++)
        {                    //select minterm
          if (covered_minterms[cur_mt]) continue;
          //cerr << "row=" << pimp << " cur=" << second_quine_table[pimp][cur_mt] << " ref=" << second_quine_table[pimp][comp_mt] << " skip=" << (!second_quine_table[pimp][cur_mt] && second_quine_table[pimp][comp_mt]) << endl;
          if (!second_quine_table[cur_pimp][cur_mt] && second_quine_table[ref_pimp][cur_mt])
          {
            cur_set = 0;
            break;
          }   //current minterm does not use p.i. which the other m.t uses so it cant domiante the other m.t column
          if (second_quine_table[cur_pimp][cur_mt] && second_quine_table[ref_pimp][cur_mt])
          {
            cur_set++;
            ref_set++;
          };
          if (second_quine_table[cur_pimp][cur_mt] && !second_quine_table[ref_pimp][cur_mt]) cur_set++;
        }
        if (cur_set != 0 && ref_set <= cur_set) skip_primeimplicants[ref_pimp] = 1;                      //mark dominant minterm column as covered
        //if(cur_set != 0 && ref_set<=cur_set) cerr << "row=" << cur_pimp << " is dominant over row=" << ref_pimp << endl;
      }
    }

    //select the primeimplicants to cover the remaining minterms
    vector<int> used_primeimplicants_list;
    for (int pimp = 0; pimp < core_primeimplicants.size(); pimp++) if (core_primeimplicants[pimp]) used_primeimplicants_list.push_back(pimp);
    for (int pimp = 0; pimp < second_quine_table.size(); pimp++)
    {
      if (core_primeimplicants[pimp] || skip_primeimplicants[pimp]) continue;
      for (int cur_mt = 0; cur_mt < minterms.size(); cur_mt++)
      {                    //select minterm
        if (covered_minterms[cur_mt]) continue;
        if (second_quine_table[pimp][cur_mt])
        {
          used_primeimplicants_list.push_back(pimp);
          break;
        }
      }
    }
    sort(used_primeimplicants_list.begin(), used_primeimplicants_list.end());

    vector<tuple<int, int, vector<int>>> used_primeimplicants;                  //class, element in class, implicants source list
    for (int pimp = 0; pimp < used_primeimplicants_list.size(); pimp++)
    {
      used_primeimplicants.push_back(source_list[pimp]);

    }

    //print and assemble the simplified equations
    for (int pimp = 0; pimp < used_primeimplicants_list.size(); pimp++)
    {
      vector<int> term;
      int temp = get<0>(source_list[used_primeimplicants_list[pimp]]), weight = 0;
      for (int j = input_size - 1; 0 <= j; j--)
      {
        if (!(get<1>(source_list[used_primeimplicants_list[pimp]]) & (1 << j)))
        {
          if (temp & (1 << j))
          {
            //cerr << "x" << j;
            term.push_back(j + 1);
          }
          else
          {
            //cerr << "!x" << j;
            term.push_back(-(j + 1));
          }
        }
      }
      //cerr << ((pimp < used_primeimplicants.size()-1)?"||":" ");
      simplified.push_back(term);
    }
    //cerr << endl;

    //cerr << endl;
  }

  int BaseMultiplierXilinxGeneralizedLUT::count_eq_dependencies(int nx, int ny, vector<vector<int>> &eq, pair<vector<int>, vector<int>> &xy_dep_list)
  {
    pair<vector<int>, vector<int>> eq_xy_dep_list;
    for (int t = 0; t < eq.size(); t++)
    {
      for (int v = 0; v < eq[t].size(); v++)
      {
        bool found_x = false, found_y = false;
        int idx = (((eq[t][v] < 0) ? -1 : 1) * eq[t][v] - 1);
        if (idx < nx)
        {
          for (int cx = 0; cx < eq_xy_dep_list.first.size(); cx++)
          {
            if (eq_xy_dep_list.first[cx] == xy_dep_list.first[idx]) found_x = true;
          }
          if (!found_x) eq_xy_dep_list.first.push_back(xy_dep_list.first[idx]);
        }
        else
        {
          for (int cy = 0; cy < eq_xy_dep_list.second.size(); cy++)
          {
            if (eq_xy_dep_list.second[cy] == xy_dep_list.second[idx - nx]) found_y = true;
          }
          if (!found_y) eq_xy_dep_list.second.push_back(xy_dep_list.second[idx - nx]);
        }
      }
    }
    //cerr << "The equation depends on " << eq_xy_dep_list.first.size()+eq_xy_dep_list.second.size() << " variables" << endl;
    return eq_xy_dep_list.first.size() + eq_xy_dep_list.second.size();
  }

  void BaseMultiplierXilinxGeneralizedLUT::print_eq(int nx, int ny, vector<vector<int>> &eq, pair<vector<int>, vector<int>> &xy_dep_list)
  {
    for (int t = 0; t < eq.size(); t++)
    {
      for (int v = 0; v < eq[t].size(); v++)
      {
        int idx = (((eq[t][v] < 0) ? -1 : 1) * eq[t][v] - 1);
        if (idx < nx)
        {
          cerr << ((eq[t][v] < 0) ? "!" : "") << "x" << xy_dep_list.first[idx] << ((v < eq[t].size() - 1) ? "&&" : "");
        }
        else
        {
          cerr << ((eq[t][v] < 0) ? "!" : "") << "y" << xy_dep_list.second[idx - nx] << ((v < eq[t].size() - 1) ? "&&" : "");
        }
      }
      cerr << ((t < eq.size() - 1) ? "||" : "");
    }
  }

  vector<int> BaseMultiplierXilinxGeneralizedLUT::get_used_vars(const vector<vector<int>> &eq1, const vector<vector<int>> &eq2)
  {
    vector<int> common_inputs;
    for (int term = 0; term < eq1.size(); term++)
    {
      for (int var = 0; var < eq1[term].size(); var++)
      {
        int com_var = 0;
        for (; com_var < common_inputs.size(); com_var++)
        {
          if (common_inputs[com_var] == abs(eq1[term][var])) break;
        }
        if (com_var == common_inputs.size()) common_inputs.push_back(abs(eq1[term][var]));
      }
    }
    for (int term = 0; term < eq2.size(); term++)
    {
      for (int var = 0; var < eq2[term].size(); var++)
      {
        int com_var = 0;
        for (; com_var < common_inputs.size(); com_var++)
        {
          if (common_inputs[com_var] == abs(eq2[term][var])) break;
        }
        if (com_var == common_inputs.size()) common_inputs.push_back(abs(eq2[term][var]));
      }
    }
    return common_inputs;
  }

  void BaseMultiplierXilinxGeneralizedLUT::count_required_LUT(vector<vector<vector<int>>> &eqs, vector<int> n_dependencys)
  {
    vector<pair<int, int>> possible_combis;
    for (int eq = 0; eq < eqs.size(); eq++)
    {
      for (int ref_eq = eq + 1; ref_eq < eqs.size(); ref_eq++)
      {
        if (eq == ref_eq) continue;
        vector<int> common_inputs{get_used_vars(eqs[eq], eqs[ref_eq])};
        if (5 < common_inputs.size()) continue;
        REPORT(LogLevel::VERBOSE, "eq" << eq << " and eq" << ref_eq << " combined use " << common_inputs.size() << " individual input variables");
        possible_combis.push_back(make_pair(eq, ref_eq));
      }
    }
    vector<pair<int, int>> best_combi;
    int max_comb_eqs = 0;
    for (int combi = 0; combi < possible_combis.size(); combi++)
    {
      vector<pair<int, int>> test_combis;
      test_combis.push_back(possible_combis[combi]);
      for (int combi1 = 0; combi1 < possible_combis.size(); combi1++)
      {
        if (combi == combi1) continue;
        int placed_combi = 0;
        for (; placed_combi < test_combis.size(); placed_combi++)
        {       //eq may not be already present
          if (test_combis[placed_combi].first == possible_combis[combi1].first) break;
          if (test_combis[placed_combi].first == possible_combis[combi1].second) break;
          if (test_combis[placed_combi].second == possible_combis[combi1].first) break;
          if (test_combis[placed_combi].second == possible_combis[combi1].second) break;
        }
        if (placed_combi == test_combis.size()) test_combis.push_back(possible_combis[combi1]);  //eqs are not already used in combination of eqs
      }
      if (max_comb_eqs < test_combis.size())
      {
        max_comb_eqs = test_combis.size();
        best_combi = test_combis;
      }
    }
    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) {
	    cerr << "could combine " << best_combi.size() << " pairs of eqs " << endl;
	    for (int placed_combi = 0; placed_combi < best_combi.size(); placed_combi++)
		    cerr << best_combi[placed_combi].first << " and " << best_combi[placed_combi].second << ", ";
    }
    int additional_lut = 0; //consider when equation does not fit in a 6LUT
    for (int eq = 0; eq < n_dependencys.size(); eq++) additional_lut += ((5 < n_dependencys[eq]) ? (1 << (n_dependencys[eq] - 6)) : 1);
    luts = additional_lut - best_combi.size();
    cost = luts + wR * 0.65; //getBitHeapCompressionCostperBit(); //Target not yet defined
    efficiency = area / cost;
    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << endl << "#6LUTs=" << luts << " cost=" << cost << " eff=" << efficiency << endl << endl;
    combined_eqs = best_combi;
  }

  void BaseMultiplierXilinxGeneralizedLUT::draw_tile(vector<vector<int>> &coverage)
  {
    for (int y = 0; y < coverage[0].size(); y++)
    {
      string line = "";
      for (int x = 0; x < coverage.size(); x++)
      {
        line = to_string(coverage[x][y]) + line;
      }
      cerr << line << endl;
    }
    //cerr << endl;
  }

  bool BaseMultiplierXilinxGeneralizedLUT::shapeValid(Parametrization const &param, unsigned x, unsigned y) const
  {
    if (0 < param.getTileXWordSize() && x < param.getTileXWordSize() && 0 < param.getTileYWordSize() && y < param.getTileYWordSize())
      if (coverage[x][y]) return true;
    return false;
  }

  bool BaseMultiplierXilinxGeneralizedLUT::shapeValid(int x, int y)
  {
    if (0 < wX && x < wX && 0 < wY && y < wY)
      if (coverage[x][y]) return true;
    return false;
  }

  Operator *BaseMultiplierXilinxGeneralizedLUT::generateOperator(
    Operator *parentOp,
    Target *target,
    Parametrization const &parameters) const
  {
    REPORT(LogLevel::VERBOSE, "tile is " << parameters.getBMC()->getCoverage().size());

    return new BaseMultiplierXilinxGeneralizedLUTOp(
      parentOp,
      target,
      parameters.isSignedMultX(),
      parameters.isSignedMultY(),
      parameters.getBMC()->getCoverage(),
      parameters.getBMC()->getDepList(),
      parameters.getBMC()->getEqs(),
      parameters.getBMC()->getCombiEqs(),
      parameters.getBMC()->getUsedOutputBits()
    );
  }

  BaseMultiplierXilinxGeneralizedLUTOp::BaseMultiplierXilinxGeneralizedLUTOp(Operator *parentOp, Target *target, bool isSignedX, bool isSignedY, const vector<vector<int>> &coverage, const pair<vector<int>, vector<int>> &xy_dependency_list,
                                                                             const vector<vector<vector<int>>> &equations, const vector<pair<int, int>> &combined_eqs, const unsigned usedOutBits) : Operator(parentOp, target),
                                                                                                                                                                                                     wX(coverage.size()),
                                                                                                                                                                                                     wY(coverage[0].size()),
                                                                                                                                                                                                     isSignedX(isSignedX), isSignedY(isSignedY),
                                                                                                                                                                                                     coverage(coverage)
  {
    ostringstream name;
    name << "BaseMultiplierXilinxGeneralizedLUT_" << wX << (isSignedX == 1 ? "_signed" : "") << "x" << wY << (isSignedY == 1 ? "_signed" : "");

    setNameWithFreqAndUID(name.str());

    addInput("X", wX);
    addInput("Y", wY);

    vector<int> out_weights, out_sizes, eqOutputMap(equations.size()), eqRelOutweight(equations.size());
    int w = 0, equa = 0;
    while ((1 << w) <= usedOutBits)
    {
      while ((usedOutBits & (1 << w)) == 0) w++;
      out_weights.push_back(w);
      int rw = 0;
      while (usedOutBits & (1 << w))
      {
        if (out_sizes.size() < out_weights.size()) out_sizes.push_back(0);
        out_sizes.back()++;
        eqOutputMap[equa] = out_weights.size() - 1;
        eqRelOutweight[equa++] = rw++;
        w++;
      }
    }

    for (int i = 0, w = 0; i < out_weights.size(); i++)
    {
      if (i == 0)
      {
        addOutput("R", out_sizes[i]);
      }
      else
      {
        addOutput(join("R", i), out_sizes[i]);
      }
    }


    int eq = 0, nlut = 0;
    unsigned long realized_eqs = 0;
    while (realized_eqs != ((1 << equations.size()) - 1))
    {
      REPORT(LogLevel::VERBOSE, "procession eq=" << eq);
      if ((realized_eqs & (1 << eq)) == 0)
      {
        for (int comb_eq = 0; comb_eq < combined_eqs.size(); comb_eq++)
        {
          if (combined_eqs[comb_eq].first == eq || combined_eqs[comb_eq].second == eq)
          {
            vector<int> lutInpMaps(xy_dependency_list.first.size() + xy_dependency_list.second.size());
            vector<int> common_vars = BaseMultiplierXilinxGeneralizedLUT::get_used_vars(equations[combined_eqs[comb_eq].first], equations[combined_eqs[comb_eq].second]);
            for (int cvars = 0; cvars < common_vars.size(); cvars++) REPORT(LogLevel::VERBOSE, to_string(common_vars[cvars]) << ", ");
            for (int cvars = 0; cvars < common_vars.size(); cvars++)
            {
              lutInpMaps[common_vars[cvars] - 1] = cvars;
            }

            lut_op lutop_o5, lutop_o6;
            REPORT(LogLevel::VERBOSE, "processing first eq " << combined_eqs[comb_eq].first);
            for (int t = 0; t < equations[combined_eqs[comb_eq].first].size(); t++)
            {
              lut_op term;
              for (int v = 0; v < equations[combined_eqs[comb_eq].first][t].size(); v++)
              {
                int idx = (((equations[combined_eqs[comb_eq].first][t][v] < 0) ? -1 : 1) * equations[combined_eqs[comb_eq].first][t][v] - 1);
                if (equations[combined_eqs[comb_eq].first][t][v] < 0)
                { //var is inverted
                  if (v == 0)
                  {
                    term = ~lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << ((t != 0) ? "||" : "") << "!i" << lutInpMaps[idx];
                  }
                  else
                  {
                    term = term & ~lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "&&!i" << lutInpMaps[idx];
                  }
                }
                else
                {
                  if (v == 0)
                  {
                    term = lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << ((t != 0) ? "||" : "") << "i" << lutInpMaps[idx];
                  }
                  else
                  {
                    term = term & lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "&&i" << lutInpMaps[idx];
                  }
                }
              }
              if (t == 0)
              {
                lutop_o5 = term;
              }
              else
              {
                lutop_o5 = lutop_o5 | term;
              }
            }
            if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << endl;
            if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "processing second eq " << combined_eqs[comb_eq].second << endl;
            for (int t = 0; t < equations[combined_eqs[comb_eq].second].size(); t++)
            {
              lut_op term;
              for (int v = 0; v < equations[combined_eqs[comb_eq].second][t].size(); v++)
              {
                int idx = (((equations[combined_eqs[comb_eq].second][t][v] < 0) ? -1 : 1) * equations[combined_eqs[comb_eq].second][t][v] - 1);
                if (equations[combined_eqs[comb_eq].second][t][v] < 0)
                { //var is inverted
                  if (v == 0)
                  {
                    term = ~lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << ((t != 0) ? "||" : "") << "!i" << lutInpMaps[idx];
                  }
                  else
                  {
                    term = term & ~lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "&&!i" << lutInpMaps[idx];
                  }
                }
                else
                {
                  if (v == 0)
                  {
                    term = lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << ((t != 0) ? "||" : "") << "i" << lutInpMaps[idx];
                  }
                  else
                  {
                    term = term & lut_in(lutInpMaps[idx]);
                    if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "&&i" << lutInpMaps[idx];
                  }
                }
              }
              if (t == 0)
              {
                lutop_o6 = term;
              }
              else
              {
                lutop_o6 = lutop_o6 | term;
              }
            }
            if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << endl;


            lut_init lutop(lutop_o5, lutop_o6);
            //cerr << lutop.truth_table() << endl;
            Xilinx_LUT6_2 *cur_lut = new Xilinx_LUT6_2(this, target);
            cur_lut->setGeneric("init", lutop.get_hex(), 64);

            if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) {
		    for (int d = 0; d < xy_dependency_list.first.size(); d++) {
			    cerr << xy_dependency_list.first[d] << ", ";
		    }
		    cerr << endl;
		    for (int d = 0; d < xy_dependency_list.second.size(); d++) {
			    cerr << xy_dependency_list.second[d] << ", ";
		    }
		    cerr << endl;
	    }

	    int cvars = 0;
            for (; cvars < common_vars.size(); cvars++)
            {
              inPortMap(join("i", cvars), ((common_vars[cvars] - 1 < xy_dependency_list.first.size()) ? join("X", of(xy_dependency_list.first[common_vars[cvars] - 1])) : join("Y", of(
                xy_dependency_list.second[common_vars[cvars] - xy_dependency_list.first.size() - 1]))));
            }
            for (; cvars < 6; cvars++)
            {
              inPortMapCst(join("i", cvars), "'0'");
            }

            inPortMapCst("i5", "'1'");
            outPortMap("o5", ((eqOutputMap[combined_eqs[comb_eq].first]) ? join("R", eqOutputMap[combined_eqs[comb_eq].first]) : "R") + of(eqRelOutweight[combined_eqs[comb_eq].first])); //"R" + of(combined_eqs[comb_eq].first));
            outPortMap("o6", ((eqOutputMap[combined_eqs[comb_eq].second]) ? join("R", eqOutputMap[combined_eqs[comb_eq].second]) : "R") + of(eqRelOutweight[combined_eqs[comb_eq].second])); //"R" + of(combined_eqs[comb_eq].second));
            vhdl << cur_lut->primitiveInstance(join("lut", nlut++)) << endl;

            realized_eqs |= (1 << combined_eqs[comb_eq].first);
            realized_eqs |= (1 << combined_eqs[comb_eq].second);
          }
        }

        // it is a 6LUT or LUT that could not be combined
        if ((realized_eqs & (1 << eq)) == 0)
        {
          lut_op lutop_o6;
          vector<int> lutInpMaps(xy_dependency_list.first.size() + xy_dependency_list.second.size());
          vector<int> common_vars = BaseMultiplierXilinxGeneralizedLUT::get_used_vars(equations[eq], {});
          for (int cvars = 0; cvars < common_vars.size(); cvars++)
          {
            lutInpMaps[common_vars[cvars] - 1] = cvars;
          }

          REPORT(LogLevel::VERBOSE, "processing single eq " << eq);
          for (int t = 0; t < equations[eq].size(); t++)
          {
            lut_op term;
            for (int v = 0; v < equations[eq][t].size(); v++)
            {
              int idx = (((equations[eq][t][v] < 0) ? -1 : 1) * equations[eq][t][v] - 1);
              if (equations[eq][t][v] < 0)
              { //var is inverted
                if (v == 0)
                {
                  term = ~lut_in(lutInpMaps[idx]);
                  if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << ((t != 0) ? "||" : "") << "!i" << lutInpMaps[idx];
                }
                else
                {
                  term = term & ~lut_in(lutInpMaps[idx]);
                  if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "&&!i" << lutInpMaps[idx];
                }
              }
              else
              {
                if (v == 0)
                {
                  term = lut_in(lutInpMaps[idx]);
                  if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << ((t != 0) ? "||" : "") << "i" << lutInpMaps[idx];
                }
                else
                {
                  term = term & lut_in(lutInpMaps[idx]);
                  if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << "&&i" << lutInpMaps[idx];
                }
              }
            }
            if (t == 0)
            {
              lutop_o6 = term;
            }
            else
            {
              lutop_o6 = lutop_o6 | term;
            }
          }
          if(LogLevel::VERBOSE <= flopoco::get_log_lvl()) cerr << endl;

          lut_init lutop(lutop_o6);
          Xilinx_LUT6 *cur_lut = new Xilinx_LUT6(this, target);
          cur_lut->setGeneric("init", lutop.get_hex(), 64);

          int cvars = 0;
          for (; cvars < common_vars.size(); cvars++)
          {
            inPortMap(join("i", cvars), ((common_vars[cvars] - 1 < xy_dependency_list.first.size()) ? join("X", of(xy_dependency_list.first[common_vars[cvars] - 1])) : join("Y", of(
              xy_dependency_list.second[common_vars[cvars] - xy_dependency_list.first.size() - 1]))));
          }
          for (; cvars < 6; cvars++)
          {
            inPortMapCst(join("i", cvars), ((cvars < 5) ? "'0'" : "'1'"));
          }

          //outPortMap("o","R" + of(eq));
          outPortMap("o", ((eqOutputMap[eq]) ? join("R", eqOutputMap[eq]) : "R") + of(eqRelOutweight[eq]));
          vhdl << cur_lut->primitiveInstance(join("lut", nlut++)) << endl;
          realized_eqs |= (1 << eq);
        }
      }
      eq++;
    }

/*        lut_op test, test1;
        for(int in=0; in<4; in++){
            test = test & lut_in(in);
        }
        test = test
        //test = lut_in(0) & lut_in(1) & lut_in(2) & lut_in(3);
        lut_init lutop( test, test1 );
        cerr << lutop.truth_table() << endl;*/

  }

  OperatorPtr BaseMultiplierXilinxGeneralizedLUT::parseArguments(OperatorPtr parentOp, Target *target, vector<string> &args, UserInterface &ui)
  {
    bool isSignedX, isSignedY;
    std::string line;
    ui.parseString(args, "wS", &line);
    ui.parseBoolean(args, "xSigned", &isSignedX);
    ui.parseBoolean(args, "ySigned", &isSignedY);

    std::vector<vector<pair<int,int>>> shapes;
    std::vector <pair<int,int>> coords;
    int next;
    while(0 <= (next = line.find(":"))){
    	std::string coordinate = line.substr(0, next);
    	line = line.substr(next+1, line.length());
    	int x = stoi(coordinate.substr(0, coordinate.find(",")));
    	int y = stoi(coordinate.substr(coordinate.find(",")+1, coordinate.length()));
    	coords.push_back(make_pair(x,y));
    }
    if(0 < coords.size()) shapes.push_back(coords);

    vector<vector<vector<int>>> shape_coverage;
    for(int i=0; i < shapes.size(); i++){
    	std::vector<vector<int>> coverage(shapes[i][0].first,vector<int>(shapes[i][0].second));
    	for(int j=1; j<shapes[i].size(); j++){
    		coverage[shapes[i][j].first][shapes[i][j].second] = 1;
    	}
    	shape_coverage.push_back(coverage);
    }
    BaseMultiplierXilinxGeneralizedLUT *tile =  new BaseMultiplierXilinxGeneralizedLUT(shape_coverage.back());
    Parametrization parameters = tile->getParametrisation();

    return new BaseMultiplierXilinxGeneralizedLUTOp(parentOp, target, parameters.isSignedMultX(),
						    parameters.isSignedMultY(),
						    parameters.getBMC()->getCoverage(),
						    parameters.getBMC()->getDepList(),
						    parameters.getBMC()->getEqs(),
						    parameters.getBMC()->getCombiEqs(),
						    parameters.getBMC()->getUsedOutputBits());
  }

  template<>
  const OperatorDescription<BaseMultiplierXilinxGeneralizedLUT> op_descriptor<BaseMultiplierXilinxGeneralizedLUT> {
    "BaseMultiplierXilinxGeneralizedLUT", // name
    "Implements a non rectangular LUT multiplier from a set that yields a relatively high efficiency compared to rectangular LUT multipliers \n",
    "Hidden", // categories
    "",
    "wS(string): colon seperated list of x,y coordinates that define the tile, the actual coordinates are preceded by the x,y dimensions of the tile;\
        xSigned(bool)=false: input X can be signed or unsigned;\
        ySigned(bool)=false: input Y can be signed or unsigned;",
    ""};

  void BaseMultiplierXilinxGeneralizedLUTOp::emulate(TestCase *tc)
  {
    mpz_class svX = tc->getInputValue("X");
    mpz_class svY = tc->getInputValue("Y");
    mpz_class svR = 0;

    svR = svX * svY;

    for (int yp = 0; yp < wY; yp++)
    {
      for (int xp = 0; xp < wX; xp++)
      {
        if (!coverage[xp][yp])
        {
          if (((isSignedX && xp == (wX - 1)) && (isSignedY && yp == (wY - 1))) || (!(isSignedX && xp == (wX - 1)) && !(isSignedY && yp == (wY - 1))))
          {
            svR -= (svX & (1 << xp)) * (svY & (1 << yp));
          }
          else
          {
            svR += (svX & (1 << xp)) * (svY & (1 << yp));
          }
        }
      }
    }

    svR &= (1 << (1 + 1)) - 1;
    svR >>= 1;
    tc->addExpectedOutput("R", svR);
  }

  TestList BaseMultiplierXilinxGeneralizedLUTOp::unitTest(int testLevel)
  {
    // the static list of mandatory tests
    TestList testStateList;
    vector<pair<string, string>> paramList;

    if(testLevel == TestLevel::QUICK)
    { // The quick tests
      paramList.push_back(make_pair("wS", "6"));
      testStateList.push_back(paramList);
      paramList.clear();
    }
    else if(testLevel >= TestLevel::SUBSTANTIAL)
    { // The substantial unit tests
      //test square multiplications:
      for (int w = 1; w <= 6; w++)
      {
        paramList.push_back(make_pair("wS", to_string(w)));
        testStateList.push_back(paramList);
        paramList.clear();
      }
    }
    return testStateList;
  }

}
