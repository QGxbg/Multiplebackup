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
    cout << "   3. failnode num [3]" << endl;
    cout << "   4. failnode ids [0,1,2]" << endl;
}



int main(int argc, char** argv) {

    if (argc < 4) {
        usage();
        return 0;
    }
    string method = string(argv[1]);
    string scenario = string(argv[2]);
    int failnodeid_size = atoi(argv[3]);
    if (argc != failnodeid_size + 4) {
        std::cout << "Invalid number of elements provided." << std::endl;
        return 1;
    }
    vector<int> failnodeids;
    for (int i = 0; i < failnodeid_size; ++i) {
        failnodeids.push_back(std::atoi(argv[i + 4]));
    }

    // cout << "Multiple failures: !!!";
    // for(auto it  : failnodeids){
    //     cout << " " << it;
    // }
    // cout << endl;

    // cout<<"!!!"<<endl;

    string configpath = "conf/sysSetting.xml";
    cout<<"Config begin"<<endl;
    Config* conf = new Config(configpath);
    cout <<"Config end"<<endl;

    int ecn = conf->_ecn;
    int eck = conf->_eck;
    int ecw = conf->_ecw;

    int batchsize = conf->_batch_size;
    
    // create stripestore
    // TODO: need to add recover from backup
    StripeStore* ss = new StripeStore(conf); 


    // coordinator
    Coordinator* coor = new Coordinator(conf, ss);

    cout<<"MultipleCoor::initRepair start"<<endl;

    coor->initRepair(method, scenario, failnodeids);
    
    cout<<"MultipleCoor::initRepair ends"<<endl;

    struct timeval time1, time2, time3, time4, time5;


    // parallel
    double latency = 0;
    vector<double> batchtime;
    
    thread genThread = thread([=]{coor->genRepairSolutionAsync();});
    
    cout<<"MultipleCoor::genRepairSolutionSaync() end"<<endl;
    
    coor->repair();
    cout<<"MultipleCoor::repair end"<<endl;

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
