#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/ECBase.hh"
#include "ec/Clay.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/HHXORPlus.hh"
#include "ec/RDP.hh"
#include "sol/SolutionBase.hh"
#include "sol/ParallelSolution.hh"
#include <dirent.h>
#include <fstream>


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
    cout << "   11. method [3]" << endl;
    cout << "   12. failnodeid_size" << endl;
    cout << "   13. failnodeids[0,1,2,3]" << endl;
}

int main(int argc, char** argv) {
    
    // if (argc != 11) {
    //     usage();
    //     return 0;
    // }

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
    int method = atoi(argv[11]);
//    int fnid = atoi(argv[10]);
    int failnodeid_size = atoi(argv[12]);
    if (argc != failnodeid_size + 13) {
        std::cout << "Invalid number of elements provided." << std::endl;
        return 1;
    }
    vector<int> failnodeids;
    for (int i = 0; i < failnodeid_size; ++i) {
        failnodeids.push_back(std::atoi(argv[i + 13]));
    }

    string config_path = "conf/sysSetting.xml";
    Config* conf = new Config(config_path);

    // 0. read block placement
    vector<Stripe*> stripelist;

    ifstream infile(filepath);
    for (int stripeid=0; stripeid<num_stripes; stripeid++) {
        string line;
        getline(infile, line);
        vector<string> items = DistUtil::splitStr(line, " ");
        vector<int> nodeidlist;
        for (int i=0; i<ecn; i++) {
            int nodeid = atoi(items[i].c_str());
            nodeidlist.push_back(nodeid);
        }
        Stripe* curstripe = new Stripe(stripeid, nodeidlist);
        stripelist.push_back(curstripe);
    }
    infile.close();


    struct timeval time1,time2,time3;
    // 1. init a solution
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
    

    //string sol_param = to_string(batchsize);
    SolutionBase* sol = new ParallelSolution(batchsize, standby_size, num_agents ,method);
    sol->init(stripelist, ec, code, conf);
    //sol->init(stripelist, ec, code, conf);

    gettimeofday(&time1,NULL);
    // 2. create a thread to generate repair batches
    //sol ->_method = method;
    sol->genRepairBatches(failnodeid_size, failnodeids, num_agents, scenario, true);
    
    gettimeofday(&time2,NULL);
    
    // 3. get repair batches
    vector<RepairBatch*> repairbatches = sol->getRepairBatches();


    //vector<RepairBatch*> repairbatches = sol->getRepairBatchesNew();
    cout << "repairbatches.size: " << repairbatches.size() << endl;

    // 4. get statistics for each batch
    int overall_load = 0;
    int overall_bdwt = 0;
    for (int batchid=0; batchid<repairbatches.size(); batchid++) {
        RepairBatch* curbatch = repairbatches[batchid];
        int load = curbatch->getLoad();
        overall_load += load;
        overall_bdwt += curbatch->getBdwt();
        
        for (auto stripe : curbatch->getStripeList()) {
            cout << "STRIPE " << stripe->getStripeId() << endl;
        //    stripe->dumpTrans(num_agents + standby_size);
            stripe->dumpLoad(num_agents + standby_size);
        }

        // curbatch->dumpLoad(num_agents+standby_size);
        // LOG <<"BATCH "<< curbatch ->getBatchId()<<endl;
        // LOG <<"BATCH load = " << load << endl;


    }
    double duration = DistUtil::duration(time1, time2);
    cout << "duration = " << duration << endl;
    cout << "[RET] overall load: "  << 1.0*overall_load/ecw << " blocks" << endl;
    cout << "[RET] overall bdwt: "  << 1.0*overall_bdwt/ecw << " blocks" << endl;
    LOG << "overall load: "  << 1.0*overall_load/ecw << " blocks" << endl;
    LOG << "overall bdwt: "  << 1.0*overall_bdwt/ecw << " blocks" << endl;

     // 内存释放（重要）
  
    for (Stripe* s : stripelist) {
        delete s;
    }
    stripelist.clear();

}
    // int overall_load = 0;
    // int overall_bdwt = 0;
    // struct timeval time1, time2, time3, time4;
    // double latency = 0;
    // vector<RepairBatch*> batch_list;
    // vector<double> latency_list;

    // vector<double> load_list;
    
    // while (sol->hasNext()) {
    //     gettimeofday(&time1, NULL);
    //     RepairBatch* curbatch = sol->getRepairBatchFromQueue();
    //     batch_list.push_back(curbatch);
    //     gettimeofday(&time2, NULL);
    //     latency += DistUtil::duration(time1, time2);
    //     latency_list.push_back(DistUtil::duration(time1,time2));

    //     int load = curbatch->getLoad();
        
    //     load_list.push_back(load);

    //     overall_load += load;
    //     LOG<<"130hang :overall_load:"<<overall_load<<endl;
    //     overall_bdwt += curbatch->getBdwt();
    //     sleep(1);
    // }
    // for(int i = 0 ; i < batch_list.size(); i++)
    // {
    //     batch_list[i]->dump(num_agents + standby_size);
    //     cout << "get batch duraiton: " << latency_list[i] << endl; 
    //     LOG << "138 hang get batch load: " << load_list[i] << endl; 

    // }
    // // join
    // genthread.join();
    // LOG << "overall load: " <<  overall_load << " subblocks" << endl;
    // LOG << "overall bdwt: " <<  overall_bdwt << " subblocks" << endl;
    // LOG << "get repair batch latency: " << latency << " ms" << endl;
