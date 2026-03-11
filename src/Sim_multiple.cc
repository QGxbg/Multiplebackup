#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/Clay.hh"
#include "ec/ECBase.hh"
#include "ec/HHXORPlus.hh"
#include "ec/RDP.hh"
#include "inc/include.hh"
#include "sol/CentSolution.hh"
#include "sol/OfflineSolution.hh"
#include "sol/ParallelSolution.hh"
#include "sol/SolutionBase.hh"
#include "util/DistUtil.hh"

using namespace std;

void usage() {
  cout << "usage: ./Simulation" << endl;
  cout << "   1. placement file path" << endl;
  cout << "   2. number of agents" << endl;
  cout << "   3. number of stripes" << endl;
  cout << "   4. code [Clay]" << endl;
  cout << "   5. ecn" << endl;
  cout << "   6. eck" << endl;
  cout << "   7, ecw" << endl;
  cout << "   8. failnode num [3]" << endl;
  cout << "   9. failnode ids [0,1,2]" << endl;
}

int main(int argc, char **argv) {

  if (argc < 8) {
    usage();
    return 0;
  }

  string filepath = argv[1];
  int num_agents = atoi(argv[2]);
  int num_stripes = atoi(argv[3]);
  string code = argv[4];
  int ecn = atoi(argv[5]);
  int eck = atoi(argv[6]);
  int ecw = atoi(argv[7]);
  string scenario = "scatter";
  int failnodeid_size = atoi(argv[8]);
  if (argc != failnodeid_size + 9) {
    std::cout << "Invalid number of elements provided." << std::endl;
    return 1;
  }
  vector<int> failnodeids;
  for (int i = 0; i < failnodeid_size; ++i) {
    failnodeids.push_back(std::atoi(argv[i + 9]));
  }
  string conf_path = "conf/sysSetting.xml";
  Config *conf = new Config(conf_path);
  // 0. read block placement
  vector<Stripe *> stripelist;
  ifstream infile(filepath);

  bool isTest = true;
  if (!isTest) {
    // 1. init a solution
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
    } else {
      cout << "Non-supported code!" << endl;
      return 0;
    }

    SolutionBase *sol = new ParallelSolution();

    // cout<<"!"<<endl;

    for (int stripeid = 0; stripeid < num_stripes; stripeid++) {
      string line;
      getline(infile, line);
      // cout<<"!!"<<endl;
      vector<string> items = DistUtil::splitStr(line, " ");
      // cout << items.size() << endl;
      vector<int> nodeidlist;
      for (int i = 0; i < ecn; i++) {
        int nodeid = atoi(items[i].c_str());
        nodeidlist.push_back(nodeid);
      }
      // cout<<"92 hang !!"<<endl;
      Stripe *curstripe = new Stripe(stripeid, nodeidlist);
      stripelist.push_back(curstripe);
    }

    // cout<<"!!!!"<<endl;
    sol->init(stripelist, ec, code, conf);

    // 2. generate repair batches
    sol->genRepairBatches(
        failnodeid_size, failnodeids, num_agents, scenario, true,
        conf->_batchmethod); // Assuming 'method' is defined elsewhere and
                             // the trailing ', 0)' was a typo.

    // 3. get repair batches
    vector<RepairBatch *> repairbatches = sol->getRepairBatches();

    // 4. get statistics for each batch
    int overall_load = 0;
    int overall_bdwt = 0;
    for (int batchid = 0; batchid < repairbatches.size(); batchid++) {
      RepairBatch *curbatch = repairbatches[batchid];
      int load = curbatch->getLoad();
      overall_load += load;
      overall_bdwt += curbatch->getBdwt();
    }

    cout << "overall load: " << overall_load / ecw << " blocks" << endl;
    cout << "overall bdwt: " << overall_bdwt / ecw << " kblocks" << endl;
  } else {
    // FIXME: test for failpattern
    unordered_map<int, vector<Stripe *>> failpattern;
    for (int stripeid = 0; stripeid < num_stripes; stripeid++) {
      int fail_num = 0;
      string line;
      getline(infile, line);
      vector<string> items = DistUtil::splitStr(line, " ");
      vector<int> nodeidlist;
      for (int i = 0; i < ecn; i++) {
        int nodeid = atoi(items[i].c_str());
        nodeidlist.push_back(nodeid);
        if (find(failnodeids.begin(), failnodeids.end(), nodeid) !=
            failnodeids.end()) {
          fail_num++;
        }
      }
      Stripe *curstripe = new Stripe(stripeid, nodeidlist);
      failpattern[fail_num].push_back(curstripe);
    }

    infile.close();

    // 1. init a solution
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
    } else {
      cout << "Non-supported code!" << endl;
      return 0;
    }

    SolutionBase *sol = new ParallelSolution();

    for (auto it : failpattern) {
      SolutionBase *sol = new ParallelSolution();
      cout << "For group failure num = " << it.first << endl;
      sol->init(it.second, ec, code, conf);
      sol->genRepairBatches(failnodeid_size, failnodeids, num_agents, scenario,
                            true, 0);
      vector<RepairBatch *> repairbatches = sol->getRepairBatches();

      int overall_load = 0;
      int overall_bdwt = 0;
      cout << repairbatches.size() << endl;
      for (int batchid = 0; batchid < repairbatches.size(); batchid++) {
        RepairBatch *curbatch = repairbatches[batchid];
        overall_load += curbatch->getLoad();
        cout << "load = " << overall_load << endl;
        overall_bdwt += curbatch->getBdwt();
        cout << "bdwt = " << overall_bdwt << endl;
      }

      LOG << "overall load: " << overall_load / ecw << " blocks" << endl;
      LOG << "overall bdwt: " << overall_bdwt / ecw << " kblocks" << endl;
    }
  }

  return 0;
}
