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
//#include "sol/ParallelSolution.hh"
#include "util/Logger.hh"

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

void simple_search(Stripe* currStripe,vector<int>& curres, int curidx,vector<int> itm_idx,vector<int> candidates,double* found,double total,
                                          unordered_map<int,int>coloring,ECDAG* ecdag, unordered_map<int,vector<int>>& max2bwlist,unordered_map<int,double>& process,
                                          struct timeval starttime,int* min_load,int* min_bdwt){
    //cout<<"32hang curidx:"<<curidx<<endl;
    if(curidx ==itm_idx.size()){
        //递归出口
        //get statistic for this solution
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
        // cout<<"37hang coloring"<<endl;
        // for(auto item:coloring_res){
        //     cout<<item.first<<":"<<item.second<<endl;
        // }
       
        currStripe->evaluateColoring();
         int load_now = currStripe->getLoad();
        int bdwt_now = currStripe->getBdwt();
        // cout<<"_load, load_now:"<< currStripe->getLoad()<<"   ,  "<<load_now<<endl;

        // cout<<"_bdwt, bdwt_now:"<< currStripe->getBdwt()<<"   ,  "<<bdwt_now<<endl;
        //cout <<  "curbw: " << bw << ", curmax: " << max << endl;
    if (max2bwlist.find(load_now) == max2bwlist.end()) {
      vector<int> curlist = {bdwt_now};
      max2bwlist.insert(make_pair(load_now, curlist));
    } else {
      vector<int> curlist = max2bwlist[load_now];
      if (find(curlist.begin(), curlist.end(), bdwt_now) == curlist.end())
        max2bwlist[load_now].push_back(bdwt_now);
    }
    if(load_now <= 4){
        LOG<<"load,bdwt = "<<load_now<<","<<bdwt_now<<endl;
        for(auto it:coloring_res){
            LOG<<it.first<<"  :"<<it.second<<endl;
        }
    }

        *found += 1;
        double perc = percentage(*found,total);

        int count = 100* perc;
        if(count % 2 ==0&& process.find(count) == process.end()){
            struct timeval t;
            gettimeofday(&t,NULL);
            double ts = DistUtil::duration(starttime,t);
            cout << "percentage: " << count << "%" << " at " << ts << endl;
            cout<<"        load="<<currStripe->_load<<"   bdwt="<<currStripe->_bdwt<<endl;
            process.insert(make_pair(count, ts));
        }
        return;
        //currStripe->_bdwt  
        //currStripe-> _load
    }
    
    for(int i=0;i<candidates.size();i++){
        int curcolor = candidates[i];
        curres[curidx] = curcolor;
        simple_search(currStripe,curres,curidx+1,itm_idx,candidates,found,total,coloring,ecdag, max2bwlist, process, starttime,min_load,min_bdwt);
        curres[curidx] = -1;
    }
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

    //Pruning
    //failnodeids.pop_back(1);
    // for(auto it:ecHeaders){
    //     cout<<it<<"  ";
    // }
    // cout<<endl;  //32767  32766 

    unordered_map<int, ECNode*>  ecmap = ecdag->getECNodeMap();
    // for(auto it : ecmap){
    //     cout<<it.first<<" : "<<it.second->getNodeId()<<endl;
    // }
    ECNode * pnode = ecmap[32766];
    pnode->removeChildNode(pnode);

    









    cout<<"color start"<<endl;

    // suppose the number of available nodes equals to n
    // idx from 0, 1, ..., n
    
    // we first color the leave nodes and header nodes
    unordered_map<int, int> coloring; 
    for(auto sidx:ecLeaves){
        int bidx = sidx / ecw;
        coloring.insert(make_pair(sidx,bidx));
    }
    //figure out header color

    for(int i=0;i<fail_num-1;i++){
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

//     cout << "itm_idx: ";
//   for (int i=0; i<itm_idx.size(); i++)
//    cout << itm_idx[i] << " ";
//   cout << endl;

//   cout << "before coloring the intermediate vertex: " << endl;
//   for (auto item: coloring) {
//     cout << "  " << item.first << ": " << item.second << endl;
//   }

//   cout << "intermediate vertex: ";
//   for (auto idx: itm_idx)
//     cout << idx <<  " ";
//   cout << endl;

//   cout << "candidates: ";
//   for (auto color: candidates) {
//     cout << color << " ";
//   }
//   cout << endl;

// The size of the solution space
  double spacesize = pow(candidates.size(), itm_idx.size());
  cout << "Spacesize: " << spacesize << endl;

  //simple search

    vector<int> curres;
    for(int i=0;i<itm_idx.size();i++)
        curres.push_back(-1);
    
    // pair<int,int> res(1111,1111);
    int min_load = 11111;
    int min_bdwt = 11111;
    // vector<int> color3 = {25,27,21,19,1,3,5,7};
    // vector<int> color2 = {28,30,20,18,2,0,4,6};

    // for(auto idx:itm_idx){
    //     if(find(color2.begin(),color2.end(),idx)!=color2.end()){
    //         coloring[idx] = 2;
    //     }
    //     if(find(color3.begin(),color3.end(),idx)!=color3.end()){
    //         coloring[idx] = 3;
    //     }
    // }
    //coloring[32766] = 1  ; 

//   cout << "after coloring the intermediate vertex: " << endl;
//   for (auto item: coloring) {
//     cout << "  " << item.first << ": " << item.second << endl;
//   }

    currStripe->setColoring(coloring);

    currStripe->evaluateColoring();
    

    
    double found=0;
    unordered_map<int, vector<int>> max2bwlist;
    unordered_map<int, double> process;
    //simple_search(currStripe,curres,0,itm_idx,candidates,&found,spacesize,coloring,ecdag, max2bwlist, process, time2,&min_load,&min_bdwt);
    

    //unordered_map<int, int> coloring; 
    //sol->genColoringForMultipleFailure(currStripe, coloring, failnodeids, num_agents, "scatter", placement);
    // currStripe->getECDAG()->dumpTOPO();
    //currStripe->getECDAG()->dump();
    //currStripe->genRepairTasks(0, ecn, eck, ecw, {});
    gettimeofday(&time3, NULL);

    double duration1 =  DistUtil::duration(time1, time2);
    double duration2 =  DistUtil::duration(time2, time3);


    int min_load_mlp = -1;
  for (auto item: max2bwlist) {
      int load = item.first;
      if (min_load_mlp == -1)
          min_load_mlp = load;
      else if (load < min_load_mlp)
          min_load_mlp = load;
  }

  int min_bdwt_mlp = -1;
  for (auto item: max2bwlist[min_load_mlp]) {
      if (min_bdwt_mlp == -1)
          min_bdwt_mlp = item;
      else if (item < min_bdwt_mlp)
          min_bdwt_mlp = item;
  }

  cout << "MLP: (" << min_load_mlp << ", " << min_bdwt_mlp << ")" << endl;
  //return 0;
    


    double source = ecdag->getECLeaves().size();
    double Layer_load = source / ecw;
    double Layer_bdwt = (source) /  ecw + fail_num -1;

    double mds_load = eck;
    double mds_bdwt = eck + fail_num - 1;

    cout << "mds_load = " << mds_load << endl;
    cout << "mds_bdwt = " << mds_bdwt << endl;

    double load = currStripe->getLoad();
    double bdwt = currStripe->getBdwt();

    cout << "load = " << load/ecw << endl;
    cout << "bdwt = " << bdwt/ecw << endl;

    cout << "layer load = " << Layer_load << endl;
    cout << "layer bdwt = " << Layer_bdwt << endl;
    cout << "Duration for gen ECDag = " << duration1 << endl; 
    cout << "Duration for Coloring = " << duration2 << endl;   
}
