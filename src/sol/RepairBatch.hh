#ifndef _REPAIRBATCH_HH_
#define _REPAIRBATCH_HH_

#include "../inc/include.hh"
#include "../ec/ECDAG.hh"
#include "../ec/NodeBatchTask.hh"
#include "../common/Stripe.hh"
#include "../common/Config.hh"
#include "../protocol/AGCommand.hh"
#include "../util/DistUtil.hh"


using namespace std;

class RepairBatch {

    private:
        int _batch_id;
        vector<Stripe*> _stripe_list;

        unordered_map<int, int> _in;
        unordered_map<int, int> _out;
        int _bdwt;
        int _load;

    public:
        // han add
        vector<Stripe*> getStripeList();
        unordered_map<int, int> getInputMap();
        unordered_map<int, int> getOutputMap();
        vector<vector<int>> getLoadTable(int num_agents);
        void push(Stripe* stripe);
        void erase(Stripe* stripe);
        Stripe* pop();
        void dumpBitVec(int);
        int getBatchId();
        void dumpLoad(int agent_num);
        // end

        RepairBatch(int id, vector<Stripe*> stripe_list);
        ~RepairBatch();
        
        void evaluateBatch(int num_agents);
        int getLoad();
        int getBdwt();
        void dump();
        void dump(int);
        void genRepairTasks(int ecn, int eck, int ecw, Config* conf,
                unordered_map<int, int> fail2repair, unsigned int coorIp);

};

#endif
