#ifndef _PARALLELSOLUTION_HH_
#define _PARALLELSOLUTION_HH_

#include "../common/TradeoffPoints.hh"
#include "../ec/ECBase.hh"
#include "SolutionBase.hh"
#include "RepairBatch.hh"
#include "unordered_set"
#include <algorithm>
#include <climits>
#include <unordered_map>
#include <random>
#include <cmath>
#include <chrono>
#include <vector>

#define DEBUG_ENABLE false 

using namespace std;


class ParallelSolution : public SolutionBase {
    private:
        TradeoffPoints* _tp;
        
        void genRepairBatchesForSingleFailure(int fail_node_id, int num_agents, string scenario);
        void genRepairBatchesForMultipleFailure(vector<int> fail_node_id, int num_agents, string scenario,int method);
        void genRepairBatchesForMultipleFailureNew(vector<int> fail_node_ids, int num_agents, string scenario,int method);
        void genRepairBatchesForMultipleFailureNewTest(vector<int> fail_node_ids, int num_agents, string scenario,int method);
        void genRepairBatchesForMultipleFailureNewFire(vector<int> fail_node_ids, int num_agents, string scenario,int method);
        
        //void genOfflineColoringForSingleFailure(Stripe* stripe, unordered_map<int, int>& res, int fail_node_id, int num_agents, string scenario);
        //void genCentralizedColoringForSingleFailure(Stripe* stripe, unordered_map<int, int>& res, int fail_node_id, int num_agents, string scenario);

    public:
        // han add

        // for hungary
        bool _debug = false;
        int _num_agents;
        int* _RepairGroup;
        int* _ifselect;
        int* _bipartite_matrix;
        int* _node_belong;
        int* _cur_matching_stripe;
        int _num_stripes_per_group;
        int* _mark;
        int _num_rebuilt_chunks;
        int* _record_stripe_id;
        int _rg_num;
        int _helpers_num;
        int _batch_size;
        int _method;
        int _num_batches;

        // xiaolu add
        bool _enqueue;

        void improve(int failnodeid, string);
        void improve_enqueue(int failnodeid, string);
        void improve_hybrid(int failnodeid, string);
        void improve_hungary(int failnodeid, string);
        void improve_multiple(vector<int> fail_node_ids,string scenario,int method);
        void improve_multipleNew(vector<int> fail_node_ids,string scenario,int method);
        
        // vector<RepairBatch*> findRepairBatchs(int soon_to_fail_node, string scenario);
        vector<RepairBatch*> findRepairBatchs(vector<int> soon_to_fail_node, string scenario);
        int if_insert(Stripe* repairstripe, int rg_id, int cur_match_stripe, vector<int> soon_to_fail_node);
        int hungary(int rg_id, int cur_match_stripe, int* matrix, int matrix_start_addr, int* node_selection);
        int greedy_replacement(int num_related_stripes, vector<int> soon_to_fail_node, int rg_id);
        int replace(int src_id, int des_id, int rg_id, int* addi_id, int num_related_stripes, string flag, vector<int> soon_to_fail_node);
        void update_bipartite_for_replace(int des_id, int stripe_id, int rg_id, int index_in_rg, string flag, vector<int> soon_to_fail_node);        
        vector<RepairBatch*> formatReconstructionSets();
        void genParallelColoringForSingleFailure(Stripe* stripe, int fail_node_id, int num_agents, string scenario, vector<vector<int>> & loadTable);
        void genParallelColoringForMultipleFailure(Stripe* stripe, vector<int> fail_node_id, int num_agents, string scenario, vector<vector<int>> & loadTable);
        
        void genOfflineColoringForSingleFailure(Stripe* stripe, int fail_node_id, int num_agents, string scenario, vector<vector<int>> & loadTable, vector<int> & placement);
        void genColoringForSingleFailure(Stripe* stripe, unordered_map<int, int>& res, int fail_node_id, int num_agents, string scenario, vector<int> & placement);
        void genColoringForMultipleFailure(Stripe* stripe, unordered_map<int, int>& res, vector<int> fail_node_id, int num_agents, string scenario, vector<int> & placement,int greedy);
        void genColoringForMultipleFailureLevel(Stripe* stripe, unordered_map<int, int>& res, vector<int> fail_node_id, int num_agents, string scenario, vector<int> & placement,int greedy);
        void genColoringForMultipleFailureLevelNew(Stripe* stripe, vector<int> fail_node_id,string scenario,vector<vector<int>> & loadTable,int method);
        void genColoringForMultipleFailureLevelNew1(Stripe* stripe, vector<int> fail_node_id,string scenario,vector<vector<int>> & loadTable,int method);
        void genColoringForMultipleFailureLevelGlobal(Stripe* stripe, unordered_map<int, int>& res, vector<int> fail_node_id, string scenario,vector<vector<int>> & loadTable,int method);

        void prepare(Stripe* stripe, vector<int> fail_node_ids, unordered_map<int, int> & res, string scenario,const vector<vector<int>> & loadTable);

        int getMax(const vector<int>& nums);
        void recalcGlobalLoad(ECDAG* ecdag, const unordered_map<int, int>& coloring, vector<int>& loads) ;
        bool tryMoveNode(const unordered_map<int, ECNode*>& nodeMap, unordered_map<int, int>& coloring, vector<int>& machine_loads, int u, int from_color, int to_color);
        void fastLocalSearch(ECDAG* ecdag, unordered_map<int, int>& coloring, const vector<int>& candidates, const vector<int>& itm_idx, int cluster_size);
        void initialGreedyColoring(ECDAG* ecdag, unordered_map<int, int>& coloring, const vector<int>& candidates, const vector<int>& topoIdxs, int cluster_size);

        
        State evalTable1(const vector<vector<int>> & table);
        State evalTable(vector<vector<int>> table, vector<int> colors);
        void GloballyMLP(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,
            ECDAG * ecdag, unordered_map<int, int> & coloring, vector<vector<int>> & loadTable);
        void GloballyMLPLevel(Stripe* stripe, const vector<int> & itm_idx,unordered_map<int, int> & coloring, const vector<int> & candidates,
            vector<vector<int>> & loadTable);
        void GloballyMLP_yh(Stripe* stripe, const vector<int> & itm_idx ,unordered_map<int, int> & coloring, vector<vector<int>> & 
                loadTable);
        int chooseColor_fullnode(Stripe* stripe,const vector<int> & childColors, const vector<vector<int>> & loadTable, int idx);
        
            void SingleMLP(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring);

        void SingleMLPAll(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring,int num_agents);
        
        void SingleMLPGroup(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring,int num_agents,unordered_map <string,vector<int>>  itm_idx_info);
        void SingleMLPLevel(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring,int num_agents,unordered_map <string,vector<int>>  itm_idx_info);

        int chooseColor1(Stripe* stripe, vector<int> childColors, unordered_map<int, int> coloring, int idx);
        int chooseColor2(Stripe* stripe, vector<int> childColors, unordered_map<int, int> coloring, vector<int> idx);
        int chooseColor4(Stripe* stripe,const vector<int> & childColors, const vector<vector<int>> & loadTable, int idx);
        int chooseColor2New(Stripe* stripe, vector<int> childColors, const vector<vector<int>> & loadTable, vector<int> itm_idx);
        
        bool isBetter(State st1, State st2);
        bool isBetter2(State st1,int color1, State st2, int color2,const vector<vector<int>> & table);
        bool isBetter3(State st1,int color1, State st2, int color2,const vector<vector<int>> & table);
        bool isBetter4(State st1,int color1, State st2, int color2,const vector<vector<int>> & table);
        Stripe* choose(vector<bool> flags, vector<Stripe*> _stripe_list);

        bool areAllNodesInLeaves(const std::vector<int>& node_parents, const std::vector<int>& ecdage_leaves);

        int count_common_nodes(const Stripe& s1, const Stripe& s2);
        vector<Stripe*> rearrange_stripes(vector<Stripe*> original_list);

        
        
        // end

        ParallelSolution();
        ParallelSolution(int batchsize, int standbysize, int agentsnum ,int method);
        void ParallelSolution_standby(int batchsize, int standbysize, int agentsnum);
        void genRepairBatches(int num_failures, vector<int> fail_node_list, int num_agents, string scenario, bool enqueue);
        //void dumpPlacement_improve(vector<RepairBatch*> batch_list);

        

};

#endif
