#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/ECBase.hh"
#include "ec/Clay.hh"
#include "ec/RDP.hh"
#include "ec/HHXORPlus.hh"
#include "ec/BUTTERFLY.hh"
#include "sol/SolutionBase.hh"
#include "sol/ParallelSolution.hh"
#include "util/Logger.hh"
using namespace std;

void usage() {
    cout << "usage: ./Simulation" << endl;
    cout << "   1. code [Clay]" << endl;
    cout << "   2. ecn" << endl;
    cout << "   3. eck" << endl;
    cout << "   4, ecw" << endl;
    cout << "   5. fail nums" << endl;
}

// 递归生成组合
void generateCombinations(vector<vector<int>>& result, vector<int>& current, int start, int n, int k) {
    if (k == 0) {
        result.push_back(current);
        return;
    }
    for (int i = start; i < n; ++i) {
        current.push_back(i);
        generateCombinations(result, current, i + 1, n, k - 1);
        current.pop_back();
    }
}

// 生成大小为 k 的不重复子集
vector<vector<int>> generateSubsets(int n, int k) {
    vector<vector<int>> result;
    vector<int> current;
    generateCombinations(result, current, 0, n, k);
    return result;
}



int main(int argc, char** argv) {
    
    if (argc != 6) {
        usage();
        return 0;
    }

    string code = argv[1];
    int ecn = atoi(argv[2]);
    int eck = atoi(argv[3]);
    int ecw = atoi(argv[4]);
    int fail_num = atoi(argv[5]);
    string conf_path = "conf/sysSetting.xml";
    Config* conf = new Config(conf_path);

    vector<vector<int>> pattern = generateSubsets(ecn, fail_num);

    cout << "All unique subsets of size " << ecn << " from 1 to " << fail_num << " are:" << endl;
    for (const auto& subset : pattern) {
        for (int num : subset) {
            cout << num << " ";
        }
        cout << endl;
    }
    
    int num_agents = ecn + fail_num;
    vector<int> placement(num_agents);
    for(int i = 0; i < num_agents; i++){
        placement[i] = i;
    }


    int stripeid = 0;

    double sum_load = 0;
    double sum_bdwt = 0;
    for(auto fail_pattern : pattern){
        
        cout << endl;
        Stripe* currStripe = new Stripe(stripeid++, placement);

        ECBase* ec;
        vector<string> param;
        if (code == "Clay") {
            ec = new Clay(ecn, eck, ecw, {to_string(ecn-1)});
        } else if (code == "RDP") {
            ec = new RDP(ecn, eck, ecw, param);
        } else if (code == "HHXORPlus") {
            ec = new HHXORPlus(ecn, eck, ecw, param);
        } else if (code == "BUTTERFLY") {
            ec = new BUTTERFLY(ecn, eck, ecw, param);
        } else {
            cout << "Non-supported code!" << endl;
            return 0;
        }
        ParallelSolution* sol = new ParallelSolution();
        sol->init({currStripe}, ec, code, conf);
        ECDAG* ecdag = currStripe->genRepairECDAG(ec, fail_pattern);
        
        unordered_map<int, int> coloring; 
        sol->genColoringForMultipleFailure(currStripe, coloring, fail_pattern, num_agents, "scatter", placement,0);
        double source = ecdag->getECLeaves().size();

        double Layer_load = source / ecw;
        double Layer_bdwt = (source) /  ecw + fail_num -1;

        double mds_load = eck;
        double mds_bdwt = eck + fail_num - 1;

        cout << "mds_load = " << mds_load << endl;
        cout << "mds_bdwt = " << mds_bdwt << endl;

        double load = currStripe->getLoad();
        double bdwt = currStripe->getBdwt();
        for(auto it : fail_pattern){
            LOG << it << "_" ;
        }
        sum_load += load / ecw;
        sum_bdwt += bdwt / ecw;
        cout << "load = " << load << endl;
        cout << "bdwt = " << bdwt << endl;
        cout << "layer load = " << Layer_load << endl;
        cout << "layer bdwt = " << Layer_bdwt << endl;
        LOG << "  " << load / ecw << " " << bdwt /ecw << endl;
        // int debug;
        // cin >> debug;
    }
    double avg_load = sum_load / pattern.size();
    double avg_bdwt = sum_bdwt / pattern.size();
    cout << "avg load = " << avg_load << endl;
    cout << "avg bdwt = " << avg_bdwt << endl;
}
