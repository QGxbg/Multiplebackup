#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/Clay.hh"
#include "ec/ECBase.hh"
#include "ec/HHXORPlus.hh"
#include "ec/RDP.hh"
#include "inc/include.hh"
#include "sol/OfflineSolution.hh"
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
  cout << "   8. scenario [standby|scatter]" << endl;
  cout << "   9. standbysize" << endl;
  cout << "   10. batchsize [3]" << endl;
  cout << "   11. fail node id [0]" << endl;
}

int main(int argc, char **argv) {

  if (argc != 12) {
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
  string scenario = argv[8];
  int standby_size = atoi(argv[9]);
  int batchsize = atoi(argv[10]);
  int fnid = atoi(argv[11]);

  string config_path = "conf/sysSetting.xml";
  Config *conf = new Config(config_path);

  // 0. read block placement
  vector<Stripe *> stripelist;
  ifstream infile(filepath);
  for (int stripeid = 0; stripeid < num_stripes; stripeid++) {
    string line;
    getline(infile, line);
    vector<string> items = DistUtil::splitStr(line, " ");
    vector<int> nodeidlist;
    for (int i = 0; i < ecn; i++) {
      int nodeid = atoi(items[i].c_str());
      nodeidlist.push_back(nodeid);
    }
    Stripe *curstripe = new Stripe(stripeid, nodeidlist);
    stripelist.push_back(curstripe);
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

  SolutionBase *sol = new OfflineSolution(batchsize, standby_size, num_agents);

  sol->init(stripelist, ec, code, conf);

  // 2. create a thread to generate repair batches
  thread genthread = thread(
      [=] { sol->genRepairBatches(1, {fnid}, num_agents, scenario, true, 0); });

  // 3. get repair batches
  int overall_load = 0;
  int overall_bdwt = 0;
  struct timeval time1, time2, time3, time4;
  double latency = 0;
  while (!sol->_finish_gen_batches) {
    gettimeofday(&time1, NULL);
    RepairBatch *curbatch = sol->getRepairBatchFromQueue();
    gettimeofday(&time2, NULL);
    latency += DistUtil::duration(time1, time2);
    int load = curbatch->getLoad();
    overall_load += load;
    overall_bdwt += curbatch->getBdwt();
  }
  // join
  genthread.join();
  cout << "overall load: " << overall_load << "= " << overall_load / ecw
       << " blocks" << endl;
  cout << "overall bdwt: " << overall_bdwt << "= " << overall_bdwt / ecw
       << " blocks" << endl;
  cout << "get repair batch latency: " << latency << " ms" << endl;
}
