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

#include "dist/Solution.hh"
#include"dist/PRSolution.hh"


using namespace std;

void usage() {
    cout << "usage: ./Simulation" << endl;
    cout << "   1. code [Clay]" << endl;
    cout << "   2. ecn" << endl;
    cout << "   3. eck" << endl;
    cout << "   4, ecw" << endl;
    cout << "   5. fail nums" << endl;
    cout << "   6. fail pattern" << endl;
}
double percentage(double found, double total) {  
    return (double)found/(double)total;
}

bool dominate(PRSolution* a, PRSolution* b) {
    int bdwt_a = a->getBdwt();
    int load_a = a->getLoad();
    int bdwt_b = b->getBdwt();
    int load_b = b->getLoad();

    if (load_a < load_b) {
        if (bdwt_a <= bdwt_b) {
            return true;
        } else {
            return false;
        }
    } else if (load_a > load_b) {
        return false;
    } else if (load_a == load_b) {
        if (bdwt_a < bdwt_b) {
            return true;
        } else 
            return false;
    }
}
bool genPareto(unordered_map<long long, long long>& pareto_curve,
        unordered_map<long long, PRSolution*>& sol_map,
        vector<int> itm_idx, vector<int> candidates,
        ECDAG* ecdag, unordered_map<int ,int> sid2ip) {

    // 0. prepare an unsearched pool
    unordered_map<long long, PRSolution*> unsearched;

    // 1. generate a solution with random colors
    int SOL_ID = 0;
    PRSolution* init_sol = new PRSolution(SOL_ID++, itm_idx.size(), candidates.size());
    init_sol->evaluate(ecdag, sid2ip, itm_idx);

    //int init_bdwt, init_load;
    //stat(sid2ip, init_sol->getSolution(), itm_idx, ecdag, &init_bdwt, &init_load);
    //init_sol->setLoad(init_load);
    //init_sol->setBdwt(init_bdwt);
    cout << "initsol: load = " << init_sol->getLoad() << ", bdwt = " << init_sol->getBdwt() << endl;

    // 1.2 add the solution into the pareto curve, which start with -1, and end
    // with -2
    pareto_curve.insert(make_pair(-1, init_sol->getId()));
    pareto_curve.insert(make_pair(init_sol->getId(), -2));
    sol_map.insert(make_pair(init_sol->getId(), init_sol));

    // 1.3 we also create a prev_curve, which start with -2, and end with -1
    unordered_map<long long, long long> prev_curve;
    prev_curve.insert(make_pair(-2, init_sol->getId()));
    prev_curve.insert(make_pair(init_sol->getId(), -1));

    // 1.4 add the solution into the unsearched pool
    unsearched.insert(make_pair(init_sol->getId(), init_sol));

    // 2. each time we get an unsearched solution from unsearched pool
    while(unsearched.size() > 0) {
        //cout << "=============================================================" << endl;
        //cout << "unsearched: " << unsearched.size() << endl;
        // 2.1 get an unsearched solution from the unsearched pool
        long long sol_id = -1;
        PRSolution* sol;
        // there is a priority, we first find unsearched solution that are
        // already in the pareto curve
        bool find = false;
        while (pareto_curve[sol_id] != -2) {
            sol_id = pareto_curve[sol_id];
            sol = sol_map[sol_id];
            
            if (sol->getSearched())
                continue;
            else {
                find = true;
                break;
            }
        }
        if (!find) {
            for (auto item: unsearched) {
                sol_id = item.first;
                sol = item.second;
                break;
            }
        }

        long long head_id = pareto_curve[-1];
        PRSolution* head_sol = sol_map[head_id];

        cout << "search for sol id: " << sol_id << ", load: " << sol->getLoad() << ", bdwt: " << sol->getBdwt() << ", headload: " << head_sol->getLoad() << ", headbdwt: " << head_sol->getBdwt() << endl;

        vector<int> itm_color = sol->getSolution();
        int v = itm_idx.size();
        int m = candidates.size();

        // 2.2 enumerate all the neighbors
        unordered_map<long long, PRSolution*> neighbors;
        for (int i=0; i<v; i++) {
            int old_color = itm_color[i];
            for (int j=0; j<m; j++) {
                int new_color = j;

                if (new_color == old_color)
                    continue;

                // exchange the old_color to the new_color
                itm_color[i] = new_color;

                // generate a neighbor solution
                PRSolution* neighbor_sol = new PRSolution(SOL_ID++, itm_idx.size(), candidates.size(), itm_color);
                neighbor_sol->evaluate(ecdag, sid2ip, itm_idx);

                //int neighbor_bdwt, neighbor_load;
                //stat(sid2ip, neighbor_sol->getSolution(), itm_idx, ecdag, &neighbor_bdwt, &neighbor_load);
                //neighbor_sol->setLoad(neighbor_load);
                //neighbor_sol->setBdwt(neighbor_bdwt);

                neighbors.insert(make_pair(neighbor_sol->getId(), neighbor_sol));
            }

            // revert the color back to old_color
            itm_color[i] = old_color;
        }
        //cout << "neighbors.size = " << neighbors.size() << endl;

        // 2.3 set sol as searched and remove from unsearched
        sol->setSearched();
        unsearched.erase(unsearched.find(sol_id));

        // 2.4 pruning neighbors with pareto curve
        // we compare each solution in neighbors with pareto curve

        // we prepare a dropped data structure to record the solution to be
        // dropped, including solutions in pareto curve, and solutions we
        // generate
        vector<long long> dropped;
        int debugnum = 0;
        //cout << "-------------------------------" << endl;
        for (auto item: neighbors) {
            long long alpha_id = item.first;
            PRSolution* alpha_sol = item.second;
            //cout << "alpha_id: " << alpha_id << ", load: " << alpha_sol->getLoad() << ", bdwt: " << alpha_sol->getBdwt() << ", pareto size: " << pareto_curve.size() << endl;


            long long beta_id = -1;
            long long prev_id, next_id = pareto_curve[beta_id];

            while (next_id != -2) {

                beta_id = next_id;
                next_id = pareto_curve[beta_id];
                prev_id = prev_curve[beta_id];

                //cout << "  beta_id = " << beta_id <<", prev_id = " << prev_id << ", next_id = " << next_id << endl;
                PRSolution* beta_sol = sol_map[beta_id];
                //cout << "    beta load: " << beta_sol->getLoad() << ", bdwt: " << beta_sol->getBdwt() << " ";

                if (dominate(beta_sol, alpha_sol)) {
                    // existing solution beta in the pareto curve dominates the
                    // generated neighbor alpha, we drop the neighbor
                    //cout << "    case 1: beta dominate alpha, drop alpha" << endl;
                    dropped.push_back(alpha_id);
                    // there is no need to compare alpha with more beta in
                    // pareto curve
                    break;
                } else if (dominate(alpha_sol, beta_sol)) {
                    // the generated neighbor alpha dominate the solution beta
                    // in the pareto curve
                    //cout << "    case 2: alpha dominate beta, update pareto curve" << endl;
                    
                    // deal with prev, beta, and alpha
                    if (prev_curve.find(alpha_id) == prev_curve.end()) {
                        pareto_curve[prev_id] = alpha_id;
                        prev_curve.insert(make_pair(alpha_id, prev_id));
                        if (sol_map.find(alpha_id) == sol_map.end())
                            sol_map.insert(make_pair(alpha_id, alpha_sol));
                    }

                    // deal with alpha, next
                    pareto_curve[alpha_id] = next_id;
                    prev_curve[next_id] = alpha_id;

                    // clear metadata for beta
                    pareto_curve.erase(pareto_curve.find(beta_id));
                    prev_curve.erase(prev_curve.find(beta_id));
                    dropped.push_back(beta_id);
                } else if (alpha_sol->getLoad() < beta_sol->getLoad()) {
                    if (prev_curve.find(alpha_id) == prev_curve.end()) {
                        //cout << "    case 3: alpha is exactly on the left of beta" << endl;
                        pareto_curve[prev_id] = alpha_id;
                        prev_curve[alpha_id] = prev_id;
                        pareto_curve[alpha_id] = beta_id;
                        prev_curve[beta_id] = alpha_id;
                        if (sol_map.find(alpha_id) == sol_map.end())
                            sol_map.insert(make_pair(alpha_id, alpha_sol));
                    }
                } else if (alpha_sol->getLoad() > beta_sol->getLoad()) {
                    if (next_id == -2) {
                        //cout << "    case 4: alpha is exactly before the tail" << endl;
                        pareto_curve[beta_id] = alpha_id;
                        prev_curve[alpha_id] = beta_id;
                        pareto_curve[alpha_id] = -2;
                        prev_curve[-2] = alpha_id;
                        if (sol_map.find(alpha_id) == sol_map.end()) {
                            sol_map.insert(make_pair(alpha_id, alpha_sol));
                        }
                    }
                } else {
                    dropped.push_back(alpha_id);
                    break;
                }
            }

            debugnum++;
            //cout << "checked " << debugnum << " neighbors" << endl;
        }

        // 2.5 remove dropped solutions
        // Note that what we add into sol_map includes sol that we add, 
        // and sol that we first add and later be removed
        for (int i=0; i<dropped.size(); i++) {
            long long drop_idx = dropped[i];
            PRSolution* sol;

            //// first check whether drop_idx is in unsearched
            //if (unsearched.find(drop_idx) != unsearched.end())
            //    continue;

            bool erased = false;
            if (neighbors.find(drop_idx) != neighbors.end()) {
                // the generated neighbor should be dropped
                sol = neighbors[drop_idx];
                neighbors.erase(neighbors.find(drop_idx));
                delete sol;
                erased = true;
            }

            if (sol_map.find(drop_idx) != sol_map.end()) {
                // the one in pareto curve should be dropped
                sol = sol_map[drop_idx];
                sol_map.erase(sol_map.find(drop_idx));
                if (!erased) {
                    delete sol;
                    erased = true;
                }
            }

            if (unsearched.find(drop_idx) != unsearched.end()) {
                unsearched.erase(unsearched.find(drop_idx));
            }
        }

        // 2.6 add remaining solutions in neighbors into unsearched pool
        //cout << "Add " << neighbors.size() << " into unsearched pool" << endl;
        for (auto item: neighbors) {
            long long add_idx = item.first;
            PRSolution* add_sol = item.second;
            unsearched.insert(make_pair(add_idx, add_sol));

            if (sol_map.find(add_idx) == sol_map.end()) {
                sol_map.insert(make_pair(add_idx, add_sol));
            }
        }
        
        cout << "unsearched pool size: " << unsearched.size() << ", pareto size: " << pareto_curve.size() << endl;
    }

    int target_load = 29;
    int target_bdwt = 184;

    bool find_better = false;
    

    // pareto curve
  long long sol_id = -1;
  PRSolution* sol;
  while (pareto_curve[sol_id] != -2) {
    
      sol_id = pareto_curve[sol_id];
      
      sol = sol_map[sol_id];

      int bdwt = sol->getBdwt();
      int load = sol->getLoad();

      if((load < target_load)|| (load == target_load && bdwt <= target_bdwt)){
        find_better =true;
      }
  }
  return find_better;

}


int main(int argc, char** argv) {
    //struct timeval time1, time2, time3;
    //gettimeofday(&time1, NULL);
    if (argc < 6) {
        usage();
        return 0;
    }

    string code = argv[1];
    int ecn = atoi(argv[2]);
    int eck = atoi(argv[3]);
    int ecw = atoi(argv[4]);
    int fail_num = atoi(argv[5]);
    if (argc != fail_num + 6) {
        std::cout << "Invalid number of elements provided." << std::endl;
        return 1;
    }
    vector<int> failnodeids;
    for (int i = 0; i < fail_num; ++i) {
        failnodeids.push_back(std::atoi(argv[i + 6]));
    }
    cout << "failnoeids: ";
    for(auto it : failnodeids){
        cout << it << " ";
    }
    cout << endl;

    string conf_path = "conf/sysSetting_example.xml";
    Config* conf = new Config(conf_path);
    int num_agents = ecn + fail_num;
    vector<int> placement(num_agents);
    for(int i = 0; i < num_agents; i++){
        placement[i] = i;
    }

    int stripeid = 0;
    double sum_load = 0;
    double sum_bdwt = 0;

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
    // ParallelSolution* sol = new ParallelSolution();
    // sol->init({currStripe}, ec, code, conf);
    ECDAG* ecdag = currStripe->genRepairECDAG(ec, failnodeids);
    //gettimeofday(&time2, NULL);

    vector<int>ecAllnodes = ecdag->getAllNodeIds();
    vector<int> ecHeaders  = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();

    int allnodes = ecdag->getAllNodeIds().size();
    int levaes = ecdag->getECLeaves().size();
    int intermediate = allnodes - levaes;
    cout << "all nodes num: " << allnodes << endl;
    cout << "leaves nodes num: " << levaes << endl;
    cout << "Intermediate nodes num : " << intermediate <<endl;

    //生成了未染色的ECDAG，开始遍历染色
    cout<<"========================================"<<endl;
    cout<<"color start"<<endl;

    // suppose the number of available nodes equals to n
    // idx from 0, 1, ..., n
    
    // we first color the leave nodes and header nodes
    unordered_map<int, int> coloring; 
    int realLeaves = 0;
    for(auto sidx:ecLeaves){
        int bidx = sidx / ecw;
        if(bidx < ecn){
            coloring.insert(make_pair(sidx,bidx));
            realLeaves++;
        }else{
            coloring.insert(make_pair(sidx,-1));
        }
    }

    cout<<"realLeaves: "<<realLeaves<<endl;

    //figure out header color

    for(int i=0;i<fail_num;i++){
        int bidx = failnodeids[i];//TODO: single first
        //cout<<"197hang :"<<bidx<<endl;
        coloring.insert(make_pair(ecHeaders[i],bidx));
        // ecHeaders[i] 
        // for(auto sidx:ecHeaders){
        //     cout<<"199 hang:"<<sidx<<endl;
        //     coloring.insert(make_pair(sidx,bidx));
        // } 
    }
    
    // now we try to color the intermediate node
    vector<int> itm_idx;
    vector<int>candidates;
    for(auto sidx: ecAllnodes){
        if(find(ecHeaders.begin(),ecHeaders.end(),sidx)!=ecHeaders.end())
            continue;
        if(find(ecLeaves.begin(),ecLeaves.end(),sidx)!=ecLeaves.end())
            continue;
        itm_idx.push_back(sidx);
        coloring.insert(make_pair(sidx,-1));
    }
    for(int i=0;i<ecn;i++){
        candidates.push_back(i);
    }
    sort(itm_idx.begin(),itm_idx.end());

    cout << "itm_idx: ";
  for (int i=0; i<itm_idx.size(); i++)
   cout << itm_idx[i] << " ";
  cout << endl;

  cout << "before coloring the intermediate vertex: " << endl;
  for (auto item: coloring) {
    cout << "  " << item.first << ": " << item.second << endl;
  }

  cout << "intermediate vertex: ";
  for (auto idx: itm_idx)
    cout << idx <<  " ";
  cout << endl;

  cout << "candidates: ";
  for (auto color: candidates) {
    cout << color << " ";
  }
  cout << endl;

// The size of the solution space
  double spacesize = pow(candidates.size(), itm_idx.size());
  cout << "Spacesize: " << spacesize << endl;
    
    
struct timeval time1, time2;
  gettimeofday(&time1, NULL);
  //Solution* mlp = genSol(itm_idx, candidates, sidx2ip, ecdag, round, w, k*w, realLeaves);
  //Solution* pareto = genPareto(itm_idx, candidates);
  bool find_better = false;


  unordered_map<long long, long long> pareto_curve;
  unordered_map<long long, PRSolution*> sol_map;

//    while (!find_better)
//  {
        // 在这里进行初始化
//    pareto_curve.clear(); // 如果需要清空原有的内容
//    sol_map.clear();      // 如果需要清空原有的内容

    find_better = genPareto(pareto_curve, sol_map, itm_idx, candidates, ecdag, coloring);
//  }

  
  gettimeofday(&time2, NULL);
  double latency = DistUtil::duration(time1, time2);
  cout << "Runtime: " << latency << endl;

  // pareto curve
  long long sol_id = -1;
  PRSolution* sol;
  while (pareto_curve[sol_id] != -2) {
    
      sol_id = pareto_curve[sol_id];
      
      sol = sol_map[sol_id];

      int bdwt = sol->getBdwt();
      int load = sol->getLoad();

      cout << "sol_id: " << sol_id << ", load: " << load << ", bdwt: " << bdwt << endl;;
      cout << "string: " << sol->getString() << endl;
  }
  cout << endl;

  return 0;

}
