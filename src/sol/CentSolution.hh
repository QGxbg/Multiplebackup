#ifndef _CENTSOLUTION_HH_
#define _CENTSOLUTION_HH_

#include "../ec/ECBase.hh"
#include "SolutionBase.hh"
#include "RepairBatch.hh"

using namespace std;

class CentSolution : public SolutionBase {

    private:
        int _batch_size;
        int _num_batches;

        void genRepairBatchesForSingleFailure(int fail_node_id, int num_agents, string scenario, bool enqueue);
        void genRepairBatchesForMultipleFailure(vector<int> fail_node_id, int num_agents, string scenario, bool enqueue);

    public:

        CentSolution();
        CentSolution(int batchsize, int standbysize, int agentsnum);

        void genRepairBatches(int num_failures, vector<int> fail_node_list, int num_agents, string scenario, bool enqueue);
        void genCentralizedColoringForSingleFailure(Stripe* stripe, unordered_map<int, int>& res, int fail_node_id, int num_agents, string scenario);
        void genCentralizedColoringForMutipleFailure(Stripe* stripe, unordered_map<int, int>& res,vector<int> fail_node_id, int num_agents, string scenario);
};

#endif
