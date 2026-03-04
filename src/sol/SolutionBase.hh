#ifndef _SOLUTIONBASE_HH_
#define _SOLUTIONBASE_HH_

#include "../common/Config.hh"
#include "../common/Stripe.hh"
#include "../inc/include.hh"
#include "../ec/ECBase.hh"
#include "../util/BlockingQueue.hh"
#include  "RepairBatch.hh"

using namespace std;

class SolutionBase {

    public:

        Config* _conf;

        // All stripe
        // idx: stripeid; 
        // value: Stripe class 
        vector<Stripe*> _stripe_list;

        // value: id of stripes in _stripe_list that needs repair
        vector<int> _stripes_to_repair;

        // batch
        vector<RepairBatch*> _batch_list;

        // targeting ec
        ECBase* _ec;
        string _codename;

        // flag
        mutex _lock;
        bool _batch_request;
        bool _finish_gen_batches;
        BlockingQueue<RepairBatch*> _batch_queue;
        
        // corresponding Solution constructor will apply
        SolutionBase();
        SolutionBase(string param);

        void init(vector<Stripe*> stripe_list, ECBase* ec, string codename, Config* conf);
        vector<RepairBatch*> getRepairBatches();
        vector<RepairBatch*> getRepairBatchesNew();
        RepairBatch* getRepairBatchFromQueue();
        bool hasNext();

        int _standby_size; 
        int _agents_num; 
        int _cluster_size; // clustersize = standbysize + agentsnum 

        // figure out stripes that requires repair
        void filterFailedStripes(vector<int> fail_node_list);

        // the following functions should be re-write in each Solution
        // we assume that replacement nodes have the same node id with fail_node_list
        // if scenario == standby, we only store repaired blocks in replacement nodes with id in fail_node_list
        // if scenario == scatter, we can store a block in any node
        // if enqueue == true, please add RepairBatch* into _batch_queue when _batch_request is set true
        virtual void genRepairBatches(int num_failures, vector<int> fail_node_list, int num_agents, string scenario, bool enqueue) = 0;
    
    };

#endif
