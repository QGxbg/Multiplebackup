#include "common/Config.hh"
#include "common/Coordinator.hh"
#include "common/StripeStore.hh"

#include "inc/include.hh"

#include "sol/RepairBatch.hh"

using namespace std;

void usage() {
    cout << "Usage: ./MultipleCoor_heterogeneous " << endl;
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

    string configpath = "conf/sysSetting.xml";
    cout<<"Config begin"<<endl;
    Config* conf = new Config(configpath);
    cout <<"Config end"<<endl;
    
    // Explicitly set to heterogeneous execution mode
    conf->_is_heterogeneous = true;

    int ecn = conf->_ecn;
    int eck = conf->_eck;
    int ecw = conf->_ecw;
    int batchsize = conf->_batch_size;
    
    // create stripestore
    StripeStore* ss = new StripeStore(conf); 

    // coordinator
    Coordinator* coor = new Coordinator(conf, ss);

    cout<<"MultipleCoor_heterogeneous::initRepair start"<<endl;

    coor->initRepair(method, scenario, failnodeids);
    
    // Explicitly configure the internal solution for heterogeneous evaluation
    coor->setHeterogeneous(true, conf->_node_bandwidths);
    
    cout<<"MultipleCoor_heterogeneous::initRepair ends"<<endl;

    // parallel
    thread genThread = thread([=]{coor->genRepairSolutionAsync();});
    
    cout<<"MultipleCoor_heterogeneous::genRepairSolutionAsync() end"<<endl;
    
    coor->repair();
    cout<<"MultipleCoor_heterogeneous::repair end"<<endl;

    genThread.join();

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
