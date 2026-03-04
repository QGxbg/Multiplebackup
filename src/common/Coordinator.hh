#ifndef _COORDINATOR_HH_
#define _COORDINATOR_HH_

#include "../inc/include.hh"
#include "../ec/ECBase.hh"
#include "../ec/Clay.hh"
#include "../ec/BUTTERFLY.hh"
#include "../ec/RDP.hh"
#include "../ec/HHXORPlus.hh"
#include "../ec/RSCONV.hh"
#include "../ec/RSPIPE.hh"
#include "../sol/SolutionBase.hh"
#include "../sol/CentSolution.hh"
#include "../sol/OfflineSolution.hh"
#include "../sol/ParallelSolution.hh"
#include "../sol/RepairBatch.hh"
#include "Config.hh"
#include "StripeStore.hh"

using namespace std;

class Coordinator {

    private:
        Config* _conf;
        StripeStore* _ss;

        string _codename;
        int _ecn;
        int _eck;
        int _ecw;

        int _blkbytes;
        int _pktbytes;
        int _agents_num;
        int _standby_size;
        int _batch_size;

        string _method;
        string _scenario;
        vector<int> _failnodeids;

        vector<RepairBatch*> _repair_batch_list;
        ECBase* _ec;
        SolutionBase* _sol;
        unordered_map<int, int> _fail2repair; // 
        

    public:
        Coordinator(Config* conf, StripeStore* ss);
        ~Coordinator();

        bool initRepair(string method, string scenario, int failnodeid);
        bool initRepair(string method, string scenario, vector<int> failnodeid);
        vector<RepairBatch*> genRepairSolution();
        int genRepairSolutionAsync();
        void repair();
};

#endif
