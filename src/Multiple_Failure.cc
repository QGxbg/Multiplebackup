#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/Clay.hh"
#include "ec/ECBase.hh"
#include "ec/HHXORPlus.hh"
#include "ec/RDP.hh"
#include "ec/RSCONV.hh"
#include "ec/RSPIPE.hh"
#include "inc/include.hh"
#include "sol/CentSolution.hh"
#include "sol/OfflineSolution.hh"
#include "sol/ParallelSolution.hh"
#include "sol/SolutionBase.hh"
#include "util/DistUtil.hh"
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
  cout << "   7. method" << endl;
  cout << "   8. scenario [standby|scatter]" << endl;
  cout << "   9. standbysize" << endl;
}

int main(int argc, char **argv) {
  struct timeval time1, time2, time3;
  gettimeofday(&time1, NULL);
  if (argc < 9) {
    usage();
    return 0;
  }

  string code = argv[1];
  int ecn = atoi(argv[2]);
  int eck = atoi(argv[3]);
  int ecw = atoi(argv[4]);
  int fail_num = atoi(argv[5]);
  if (argc != fail_num + 9) {
    std::cout << "Invalid number of elements provided." << std::endl;
    return 1;
  }
  string scenario = argv[fail_num + 7];
  int standby_size = atoi(argv[fail_num + 8]);

  int ecq = ecn - eck;
  int ect = log(ecw) / log(ecq);
  int ecnu = (ecn - eck) * ect - ecn;

  vector<int> failnodeids;
  for (int i = 0; i < fail_num; ++i) {
    failnodeids.push_back(std::atoi(argv[i + 6]));
  }
  cout << "failnoeids: ";
  for (auto it : failnodeids) {
    cout << it << " ";
  }
  cout << endl;
  int method = 0;
  method = atoi(argv[fail_num + 6]);
  // cout<<method<<endl;

  string conf_path = "conf/sysSetting_example.xml";
  Config *conf = new Config(conf_path);
  int num_agents = ecn + fail_num; // 真正的agent_num
  // int num_agents = ecn + standby_size; //standby_size / fail_num 替代了
  vector<int> placement(num_agents);
  for (int i = 0; i < num_agents; i++) {
    placement[i] = i;
  }
  vector<int> nodeidlist;
  for (int i = 0; i < ecn; i++) {
    int nodeid = i;
    nodeidlist.push_back(nodeid);
  }
  // Stripe* curstripe = new Stripe(stripeid, nodeidlist);

  int stripeid = 0;
  double sum_load = 0;
  double sum_bdwt = 0;

  Stripe *currStripe = new Stripe(stripeid++, nodeidlist);
  ECBase *ec;
  vector<string> param;
  if (code == "Clay") {
    ec = new Clay(ecn, eck, ecw, {to_string(ecn - 1)});
  } else if (code == "RDP") {
    ec = new RDP(ecn, eck, ecw, param);
  } else if (code == "HHXORPlus") {
    ec = new HHXORPlus(ecn, eck, ecw, param);
  } else if (code == "BUTTERFLY") {
    ec = new BUTTERFLY(ecn, eck, ecw, param);
  } else if (code == "RSCONV") {
    ec = new RSCONV(ecn, eck, ecw, param);
  } else if (code == "RSPIPE") {
    ec = new RSPIPE(ecn, eck, ecw, param);
  } else {
    cout << "Non-supported code!" << endl;
    return 0;
  }
  ParallelSolution *sol =
      new ParallelSolution(1, standby_size, num_agents, method);
  sol->init({currStripe}, ec, code, conf);

  // OfflineSolution *sol_offline =
  //     new OfflineSolution(1, standby_size, num_agents);
  // sol_offline->init({currStripe}, ec, code, conf);

  // CentSolution *sol_cent = new CentSolution(1, standby_size, num_agents);
  // sol_cent->init({currStripe}, ec, code, conf);

  ECDAG *ecdag = currStripe->genRepairECDAG(ec, failnodeids);
  currStripe->refreshECDAG(ecdag);
  // cout << "ecdag -> dump:" << endl;
  // ecdag->dump();
  // ecdag->dumpTOPO();

  cout << "------------------------------" << endl;
  // cout<<"ecdag->dumpTopo"<<endl;
  // ecdag -> dumpTOPO();

  gettimeofday(&time2, NULL);
  vector<int> ecAllnodes = ecdag->getAllNodeIds();
  vector<int> ecHeaders = ecdag->getECHeaders();
  vector<int> ecLeaves = ecdag->getECLeaves();

  int allnodes = ecdag->getAllNodeIds().size();
  int levaes = ecdag->getECLeaves().size();
  int intermediate = allnodes - levaes;
  cout << "all nodes num: " << allnodes << endl;
  cout << "leaves nodes num: " << levaes << endl;
  cout << "Intermediate nodes num : " << intermediate << endl;

  // now we try to color the intermediate node
  vector<int> itm_idx;
  vector<int> candidates;
  for (auto sidx : ecAllnodes) {
    if (find(ecHeaders.begin(), ecHeaders.end(), sidx) != ecHeaders.end())
      continue;
    if (find(ecLeaves.begin(), ecLeaves.end(), sidx) != ecLeaves.end())
      continue;
    itm_idx.push_back(sidx);
  }
  for (int i = 0; i < ecn; i++) {
    candidates.push_back(i);
  }
  sort(itm_idx.begin(), itm_idx.end());

  //     cout << "itm_idx: ";
  //   for (int i=0; i<itm_idx.size(); i++)
  //    cout << itm_idx[i] << " ";
  //   cout << endl;

  //   cout << "intermediate vertex: ";
  //   for (auto idx: itm_idx)
  //     cout << idx <<  " ";
  //   cout << endl;
  // cout<<"sol->_cluster_size:"<<sol->_cluster_size<<endl;
  // sol_offline->_cluster_size = num_agents;
  // sol_cent->_cluster_size = num_agents;
  //  vector<vector<int>> loadtable = vector<vector<int>> (sol->_cluster_size,
  //  {0,0});
  vector<vector<int>> loadtable =
      vector<vector<int>>(sol->_cluster_size, {0, 0});

  unordered_map<int, int> coloring;
  double load = 0;

  if (fail_num == 1) {
    sol->genColoringForSingleFailure(currStripe, coloring, failnodeids[0],
                                     num_agents, scenario, placement);
  } else {
    //     //sol->genColoringForMultipleFailureLevel(currStripe, coloring,
    //     failnodeids, num_agents, scenario, placement,method);
    sol->genColoringForMultipleFailureLevelNew(currStripe, failnodeids,
                                               scenario, loadtable, method);
    load = currStripe->getLoad();
    cout << "currStripe load :" << currStripe->getLoad() << endl;
    cout << "currStripe bdwt :" << currStripe->getBdwt() << endl;
  }
  // sol->genColoringForMultipleFailureLevel(currStripe, coloring, failnodeids,
  // num_agents, "scatter", placement,greedy);

  // sol_offline->genOfflineColoringForMultipleFailure(
  //     currStripe, coloring, failnodeids, num_agents, "scatter");

  // string offline_solution_path = conf->_tpDir + "/" + code + "_" +
  //                                to_string(ecn) + "_" + to_string(eck) + "_"
  //                                + to_string(ecw) + ".xml";
  // cout << offline_solution_path << endl;
  // TradeoffPoints *tp = new TradeoffPoints(offline_solution_path);
  // OfflineSolution* os = (OfflineSolution*)sol;
  // sol_offline->setTradeoffPoints(tp);

  // sol_offline->genOfflineColoringForMultipleFailure(
  //     currStripe, coloring, failnodeids, num_agents, "scatter");

  // sol_cent->setTradeoffPoints(tp);
  // sol_cent->genCentralizedColoringForMutipleFailure(
  //     currStripe, coloring, failnodeids, num_agents, "scatter");

  // for (auto it : coloring) {
  //   LOG << it.first << ":" << it.second << endl;
  // }

  // currStripe->setColoring(coloring);
  currStripe->evaluateColoring();

  // LOG << "after genColoringForMultipleFailureLevel " << endl;
  // currStripe->getECDAG()->dumpTOPO();
  // currStripe->getECDAG()->dump();
  // currStripe->genRepairTasks(0, ecn, eck, ecw, {});
  gettimeofday(&time3, NULL);

  double duration1 = DistUtil::duration(time1, time2);
  double duration2 = DistUtil::duration(time2, time3);

  double source = ecdag->getECLeaves().size();
  double Layer_load = source / ecw;
  double Layer_bdwt = (source) / ecw + fail_num - 1;

  double mds_load = eck;
  double mds_bdwt = eck + fail_num - 1;

  cout << "mds_load = " << mds_load << endl;
  cout << "mds_bdwt = " << mds_bdwt << endl;

  // currStripe ->dumpLoad(sol_offline->_cluster_size);

  load = currStripe->getLoad();
  double bdwt = currStripe->getBdwt();

  cout << "load = " << load / ecw << endl;
  cout << "bdwt = " << bdwt / ecw << endl;

  cout << "layer load = " << Layer_load << endl;
  cout << "layer bdwt = " << Layer_bdwt << endl;
  cout << "Duration for gen ECDag = " << duration1 << endl;
  cout << "Duration for Coloring = " << duration2 << endl;

  currStripe->dumpLoad(num_agents);

  // cout << load/ecw << endl;
}
