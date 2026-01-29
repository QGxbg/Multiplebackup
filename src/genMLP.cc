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

bool dominate(Solution* A, Solution* B) {
    // returns true if A dominates B
    int loada = A->getLoad();
    int bdwta = A->getBdwt();

    int loadb = B->getLoad();
    int bdwtb = B->getBdwt();

    if (loada < loadb) {
        if (bdwta < bdwtb) {
            return true;
        } else if (bdwta == bdwtb) {
            return true;
        } else {
            return false;
        }
    } else if (loada > loadb) {
        if (bdwta < bdwtb) {
            return false;
        } else if (bdwta == bdwtb) {
            return false;
        } else {
            return false;
        }
    } else if (loada == loadb) {
        if (bdwta < bdwtb) {
            return true;
        } else if (bdwta == bdwtb) {
            return false;
        } else {
            return false;
        }
    }

}

bool updateTradeoffCurve(Solution* head, Solution* tail, Solution* sol, 
        unordered_map<int, int>& load2bdwt, unordered_map<int, int>& bdwt2load) {

    bool status = false;
    int sol_load = sol->getLoad();
    int sol_bdwt = sol->getBdwt();

    Solution* current = head;

    current = head;
    //cout << "before:--------------------";
    //while(current) {
    //    cout << current->getString() << "; ";
    //    current = current->getNext();
    //}
    //cout << endl;

    current = head;
    Solution* prev = NULL;
    Solution* next = NULL;
    vector<Solution*> toremove;
    //cout << "          check tradeoff curve for " << sol->getString() << endl;

    while (current->getNext() != tail) {
        current = current->getNext();
        if (current == sol) {
            //cout << "            current == sol == " << sol->getString() << ", continue;" << endl;
            continue;
        } else if (current->getString() == sol->getString()) {
            //cout << "            current == sol == " << sol->getString() << ", break;" << endl;
            break;
        }

        int cur_load = current->getLoad();
        int cur_bdwt = current->getBdwt();
        //cout << "            cmp with: " << current->getString() << ", load: " << current->getLoad() << ", bdwt: " << current->getBdwt() 
        //    << ", prev: " << current->getPrev()->getString() << ", next: " << current->getNext()->getString() << endl;

        // 1. current dominates sol
        if (dominate(current, sol)) {
            // we cannot update 
            //cout << "              current dominate sol" << endl;
            break;
        }

        // 2. sol dominates current
        if (dominate(sol, current)) {
            // update prev if this is the first point we find that is dominated
            // by sol
            //cout << "              sol dominate current, current->prev: " << current->getPrev()->getString() << ", current->next: " << current->getNext()->getString() << endl;
            if (prev == NULL) {
                prev = current->getPrev();
                prev->setNext(sol);
                sol->setPrev(prev);
                current->setPrev(NULL);
                //cout << "                update sol->prev with current->prev, sol->prev: " << sol->getPrev()->getString() << endl;
            }

            // update next
            next = current->getNext();
            //cout << "                current->next: " << next->getString() << endl;
            sol->setNext(next);
            next->setPrev(sol);
            //cout << "                update sol->next with current->next, sol->next: " << sol->getNext()->getString() << endl;

            load2bdwt.erase(cur_load);
            bdwt2load.erase(cur_bdwt);

            load2bdwt.insert(make_pair(sol_load, sol_bdwt));
            bdwt2load.insert(make_pair(sol_bdwt, sol_load));

            toremove.push_back(current);
            status = true;
        }

        // 3. current and sol cannot dominate each other
        if (!dominate(sol, current) && !dominate(current, sol)) {
            status = true;
            // we need to figure out to add sol before current or behind
            //cout << "              sol and current cannot dominate each other" << endl;
            if (sol_load < cur_load) {
                if (prev == NULL) {
                    // add sol before current, and sol hasn't been inserted
                    //cout << "                insert sol before current" << endl;
                    prev = current->getPrev();
                    prev->setNext(sol);
                    sol->setPrev(prev);
                    sol->setNext(current);
                    current->setPrev(sol);

                    load2bdwt.insert(make_pair(sol_load, sol_bdwt));
                    bdwt2load.insert(make_pair(sol_bdwt, sol_load));
                }
            } else {
                // add sol after current
                next = current->getNext();
                if (next == tail) {
                    current->setNext(sol);
                    sol->setPrev(current);
                    sol->setNext(next);
                    next->setPrev(sol);
                    load2bdwt.insert(make_pair(sol_load, sol_bdwt));
                    bdwt2load.insert(make_pair(sol_bdwt, sol_load));
                    //cout << "                insert sol at the tail, sol->prev: " << sol->getPrev()->getString() << ", sol: " << sol->getString() << ", sol->next: " << sol->getNext()->getString() << endl;
                    //cout << "                now, current->prev: " << current->getPrev()->getString() << ", current: " << current->getString() << ", current->next: " << current->getNext()->getString() << endl;
                }
            }
        }
    }

    for (auto item: toremove)
        delete item;

    // debug
    current = head;
    //cout << "after:--------------------";
    //while(current) {
    //    cout << current->getString() << "; ";
    //    current = current->getNext();
    //}
    //cout << endl;

    return status;
}


void expand(Stripe* currStripe,Solution*current,int v,int m,unordered_map<string, bool>& visited,
        unordered_map<int, int> coloring, vector<int> itm_idx, ECDAG* ecdag,
        unordered_map<int, int>& load2bdwt, unordered_map<int, int>& bdwt2load,
        Solution* tradeoff_curve_head, Solution* tradeoff_curve_tail) {
    
    vector<int> solution = current ->getSolution();
    current->setExpanded(true);

    for(int i = 0;i<v;i++){
        int oldv = solution[i];
        for(int j=0;j<m;j++){
            if(j == oldv)
                continue;
            // update the color of one vertex
            //cout<<"i,j"<<i<<","<<j<<endl;
            solution[i] = j;
            Solution* neighbor = new Solution(v, m, solution);
            string tmps = neighbor->getString();

            Solution* current = tradeoff_curve_head;

            // check whether the neighbor has been visited
            if (visited.find(tmps) != visited.end()) {
                // this solution has been visited
                //cout << "        visited, skip" << endl;
                delete neighbor;
            } else {
                // this solution hasn't been visited before

                // get stat for the neighbor
                int neighbor_bdwt, neighbor_load;
                vector<int> curres = neighbor ->getSolution();
                //cout<<"223 hang neighbor   curres:"<<tmps <<endl;
                unordered_map<int, int> coloring_res;
                for (auto item: coloring) {
                    coloring_res.insert(make_pair(item.first, item.second));
                }
                for (int ii=0; ii<curres.size(); ii++) {
                    int idx = itm_idx[ii];
                    int color = curres[ii];
                    coloring_res[idx] = color;
                }
                currStripe->setColoring(coloring_res);
                currStripe ->evaluateColoring();

                //cout << "        neighbor: load = " << currStripe->_load << ", bdwt = " << currStripe->_bdwt << ", string = " << neighbor->getString() << endl;


                neighbor ->setBdwt(currStripe->_bdwt);
                neighbor->setLoad(currStripe->_load);

                neighbor_bdwt = currStripe->_bdwt;
                neighbor_load = currStripe->_load;    

                visited.insert(make_pair(tmps,true));
                // now we check whether the current neighbor can update
                // the tradeoff_curve. we first do a quick search
                if (load2bdwt.find(neighbor_load) != load2bdwt.end()) {
                    // a point of the same load exists in the tradeoff curve
                    if (load2bdwt[neighbor_load] <= neighbor_bdwt) {
                        // neighbor cannot update the tradeoff_curve
                        //cout << "          curve bdwt: " << load2bdwt[neighbor_load] << " is better, skip the neighbor" << endl;
                        delete neighbor;
                        neighbor = NULL;
                    }
                } else if (bdwt2load.find(neighbor_bdwt) != bdwt2load.end()) {
                    // a point of the same bdwt exists in the tradeoff curve
                    if (bdwt2load[neighbor_bdwt] <= neighbor_load) {
                        // neighbor cannot update the tradeoff curve
                        //cout << "          curve load: " << bdwt2load[neighbor_bdwt] << " is better, skip the neighbor" << endl;
                        delete neighbor;
                        neighbor = NULL;
                    }
                }
                // neighbor_bdwt or neighbor_load is new
                // we iterate the tradeoff curve to insert the neighbor and
                // update two maps
                if (neighbor) {
                    if(!updateTradeoffCurve(tradeoff_curve_head, tradeoff_curve_tail, neighbor, load2bdwt, bdwt2load)) {
                        delete neighbor;
                        neighbor = NULL;
                    } else {
                        //cout << "update tradeoff curve at i: " << i << ", j: " << j << endl;
                    }
                }
                current = tradeoff_curve_head;
            }
        }
        solution[i]= oldv;
    }
}





Solution* genTradeoffCurve(Stripe* currStripe,vector<int> itm_idx,vector<int> candidates,
                                   unordered_map<int, int> coloring, ECDAG* ecdag,int round){

    // 0. initialize a head and tail with NULL for the tradeoff curve
    cout<<"genTradeoffCurve start"<<endl;
    Solution* head = new Solution(true);  //_type = 0;
    Solution* tail = new Solution(false);  //_type = 1;

    head ->setNext(tail);
    tail ->setPrev(head);
    
    // 1. randomly select a solution and insert it into the tradeoff curve
    Solution* init_sol = new Solution(itm_idx.size(), candidates.size(),round);

    head->setNext(init_sol);
    init_sol->setPrev(head);
    init_sol->setNext(tail);
    tail->setPrev(init_sol);

    // 1.1 get stat for the init_sol

    int v = itm_idx.size();
    int m = candidates.size();
    int init_bdwt, init_load;

    vector<int> curres = init_sol ->getSolution();
    unordered_map<int, int> coloring_res;
    for (auto item: coloring) {
        coloring_res.insert(make_pair(item.first, item.second));
    }
    for (int ii=0; ii<curres.size(); ii++) {
        int idx = itm_idx[ii];
        int color = curres[ii];
        coloring_res[idx] = color;
    }
    currStripe->setColoring(coloring_res);
    currStripe ->evaluateColoring();

    init_sol ->setBdwt(currStripe->_bdwt);
    init_sol->setLoad(currStripe->_load);

    init_bdwt = currStripe->_bdwt;
    init_load = currStripe->_load;

    cout << "genTradeoffCurve1: init_sol: " << init_sol->getString() << ", load: " << init_sol->getLoad() << ", bdwt: " << init_sol->getBdwt() << endl;

    // 2. generate a map that records the solution that we visited.
    unordered_map<string,bool> visited;
    visited.insert(make_pair(init_sol->getString(),true));

    // 3. generate load2bdwt and bdwt2load map
    unordered_map<int, int> load2bdwt;
    unordered_map<int, int> bdwt2load;
    load2bdwt.insert(make_pair(init_load, init_bdwt));
    bdwt2load.insert(make_pair(init_bdwt, init_load));
    
    // 4. each time we choose the first solution in the tradeoff curve that
    // hasn't been expanded
    Solution* current;
    while(true){
        current = head -> getNext();
        //cout << "head: load = " << current->getLoad() << ", bdwt = " << current->getBdwt() << ", string = " << current->getString() << endl;

        // find the first solution that not expanded
        while(current != tail){
            if(current->getExpanded()){
                //cout << "    expanded, skip" << endl;
                current = current->getNext();
            }else{
                //cout << "    not expanded" << endl;
                break;
            }
        }

        if(current == tail){
            // all the solutions has been expanded
            //cout << "  reach the tail" << endl;
            break;
        }

        // now we expand the current solution
        expand(currStripe,current,v,m,visited,coloring,itm_idx,ecdag,load2bdwt,bdwt2load,head,tail);
    }

    return head;


}

int getLength(Solution* head) {
    int size=0;
    Solution* cur = head;
    while (cur != NULL) {
        cout << "get Length load: " << cur->getLoad() << ", bdwt: " << cur->getBdwt() << endl;
        size++;
        cur = cur->getNext();
    }

    return size;
}


Solution* genSol(Stripe* currStripe,vector<int> itm_idx,vector<int> candidates,
                                   unordered_map<int, int> coloring, ECDAG* ecdag,
                                    int rounds, int target_load, int target_bdwt, int conv){
    Solution* sol =NULL;
    Solution* head;
    Solution* s;
    Solution* cur;
    Solution* next;
    bool init = false;


    
    int i=0;
    int a=0;
    while(true){
        a++;
        head = genTradeoffCurve(currStripe,itm_idx, candidates, coloring, ecdag,a);
        bool find = false;
        s ==NULL;
        // iterate the tradeoff line
        cur = head->getNext();
        while (cur) {
            int load = cur->getLoad();
            int bdwt = cur->getBdwt();
            //cout << "393 hang load: " << load << ", bdwt: " << bdwt << endl;

            if (load == 0 && bdwt == 0) {
                // reach the tail
                delete cur;
                break;
            } 

            // check whether current solution is good enough
            if (load >= target_load &&  load <= 11 &&bdwt <=46 && bdwt >= conv) {
                //cout << "  yes" << endl;
                find = true;
                s = cur;

                // now we delete the remainings
                cur = cur->getNext();
                while (cur) {
                    next = cur->getNext();
                    delete cur;
                    cur = next;
                }

            } else {
                //cout << "  no" << endl;
                // the current solution is worse than ecpipe
                next = cur->getNext();
                delete cur;
                cur = next;
            }

        }

        //  now we find a solution, compare it with sol
        if (find) {
            if (sol == NULL)
                sol = s;
            else {
                if (s->getLoad() < sol->getLoad()) {
                    delete sol;
                    sol = s;
                } else {
                    delete s;
                }
            }
        }

        if (sol == NULL) {
            //cout <<  "-----" << endl;
        } else {
            cout << "  sol: load =  " << sol->getLoad() << ", bdwt: " << sol->getBdwt() << endl;
        }

        i++;

        if (find && i>rounds) {
            break;
        }

    }

    cout << "453 hang i = " << i-1 << endl;

    // int size = getLength(sol);
    // cout<<"645hang size:"<<size<<endl;

    return sol;

}




int main(int argc, char** argv) {
    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);
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
    gettimeofday(&time2, NULL);

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

  //simple search

    vector<int> curres;
    for(int i=0;i<itm_idx.size();i++)
        curres.push_back(-1);
    

    
    double found=0;
    unordered_map<int, vector<int>> max2bwlist;
    unordered_map<int, double> process;
    //simple_search(currStripe,curres,0,itm_idx,candidates,&found,spacesize,coloring,ecdag, max2bwlist, process, time2,&min_load,&min_bdwt);
    
    int round = ecn*ecw;
    if(eck >= 6 && eck < 8){
        round = 10;
    }
    if (eck >= 8) round =1;

    round = 1;

    cout<<"round = "<<round<<endl;

    Solution* mlp = genSol(currStripe,itm_idx, candidates, coloring, ecdag, round, ecw, fail_num*eck*ecw, realLeaves);
    // = genSol()    

    


    //unordered_map<int, int> coloring; 
    //sol->genColoringForMultipleFailure(currStripe, coloring, failnodeids, num_agents, "scatter", placement);
    // currStripe->getECDAG()->dumpTOPO();
    //currStripe->getECDAG()->dump();
    //currStripe->genRepairTasks(0, ecn, eck, ecw, {});
    gettimeofday(&time3, NULL);

    double duration1 =  DistUtil::duration(time1, time2);
    double duration2 =  DistUtil::duration(time2, time3);
    


    double source = ecdag->getECLeaves().size();
    double Layer_load = source / ecw;
    double Layer_bdwt = (source) /  ecw + fail_num -1;

    double mds_load = eck;
    double mds_bdwt = eck + fail_num - 1;

    cout << "mds_load = " << mds_load << endl;
    cout << "mds_bdwt = " << mds_bdwt << endl;

    // double load = currStripe->getLoad();
    // double bdwt = currStripe->getBdwt();

    // cout << "load = " << load/ecw << endl;
    // cout << "bdwt = " << bdwt/ecw << endl;

    cout << "layer load = " << Layer_load << endl;
    cout << "layer bdwt = " << Layer_bdwt << endl;
    cout << "Duration for gen ECDag = " << duration1 << endl; 
    cout << "Duration for Coloring = " << duration2 << endl;   


    //print the mlp
    cout<<"Digits: "<<mlp->getDigits()<<",string: " << mlp->getString() << endl;

    double load = mlp->getLoad();
    double bdwt = mlp->getBdwt();

    cout << "load = " << load/ecw << endl;
    cout << "bdwt = " << bdwt/ecw << endl;

}
