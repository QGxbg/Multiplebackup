//#include "common/CmdDistributor.hh"
#include "common/Config.hh"
#include "common/Coordinator.hh"
#include "common/StripeStore.hh"

#include "inc/include.hh"

#include "sol/RepairBatch.hh"

using namespace std;

void usage() {
    cout << "Usage: ./ParaCoordinator " << endl;
    cout << "   1. method [centralize|offline|parallel]" << endl;
    cout << "   2. scenario [standby|scatter]" << endl;
    cout << "   3. failnodeid [0]" << endl;
}

int main(int argc, char** argv) {

    if (argc < 4) {
        usage();
        return 0;
    }
    string method = string(argv[1]);
    string scenario = string(argv[2]);
    int failnodeid = atoi(argv[3]);
    string configpath = "conf/sysSetting.xml";
    Config* conf = new Config(configpath);
    int ecn = conf->_ecn;
    int eck = conf->_eck;
    int ecw = conf->_ecw;
    
    // create stripestore
    // TODO: need to add recover from backup
    StripeStore* ss = new StripeStore(conf); 
    // coordinator
    Coordinator* coor = new Coordinator(conf, ss);
    coor->initRepair(method, scenario, failnodeid);
    struct timeval time1, time2, time3, time4, time5;

    // parallel
    double latency = 0;
    vector<double> batchtime;
    thread genThread = thread([=]{coor->genRepairSolutionAsync();});
    sleep(2);
    coor->repair();
    genThread.join();

   // clean
   // clean coordinator
   if (coor)
       delete coor;
   // clean stripe store
   if (ss)
       delete ss;
   // clean conf
   if (conf)
       delete conf;

    return 0;
}
