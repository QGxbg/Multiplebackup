#ifndef _STRIPE_HH_
#define _STRIPE_HH_

#include "../inc/include.hh"
#include "../ec/ECBase.hh"
#include "../ec/ECDAG.hh"
#include "../ec/Task.hh"

using namespace std;
#define IN 1
#define OUT 0

class State{
    public:
    int _load = 0;
    int _bdwt = 0;
    State(int load, int bdwt){
        _load = load;
        _bdwt = bdwt;
    }
};

class Stripe {

    private:
        int _stripe_id;

        // node that stores blocks of this stripe
        

        // the ecdag to repair a block
        ECDAG* _ecdag;

        // map a sub-packet idx to a **real physical** node id
        
        
        // statistics
        // for each node, count the number of sub-packets that it receives
        unordered_map<int, int> _in; 
        // for each node, count the number of sub-packets that it sends
        unordered_map<int, int> _out;



        // for prototype
        string _stripe_name;
        vector<string> _blklist;
        vector<unsigned int> _loclist;
        unordered_map<int, vector<Task*>> _taskmap;

        // for single failure
        int _fail_blk_idx;
        


    public:
        Stripe(int stripeid, vector<int> nodelist);
        Stripe(int stripeid, string stripename, vector<string> blklist, vector<unsigned int> loclist, vector<int> nodelist);
        ~Stripe();
        vector<int> getPlacement();
        int getStripeId();
        int getFailNum();
        // string getCodeName();

        double _bdwt;
        double _load;
        
        // for multiple failure
        vector<int> _fail_blk_idxs;

        vector<int> _nodelist;

        unordered_map<int, int> _coloring;


        // ECDAG* genRepairECDAG(ECBase* ec, int fail_node_id);
        ECDAG* genRepairECDAG(ECBase* ec, vector<int> fail_node_id);
        ECDAG* getECDAG();
        
        void setColoring(unordered_map<int, int> coloring);
        unordered_map<int, int> getColoring();

        void evaluateColoring();
        void refreshECDAG(ECDAG * ecdag);

        unordered_map<int, int> getInMap();
        unordered_map<int, int> getOutMap();
        int getBdwt();
        int getLoad();

        unordered_map<int, vector<Task*>> genRepairTasks(int batchid, int ecn, int eck, int ecw, unordered_map<int, int> fail2repair);
        void genLevel0(int batchid, int ecn, int eck, int ecw, vector<int> leaves, unordered_map<int, vector<Task*>>& res);
        void genIntermediateLevels(int batchid, int ecn, int eck, int ecw, vector<int> leaves, unordered_map<int, vector<Task*>>& res);
        void genLastLevel(int batchid, int ecn, int eck, int ecw, vector<int> leaves, unordered_map<int, vector<Task*>>& res);

        int getTaskNumForNodeId(int nodeid);
        vector<Task*> getTaskForNodeId(int nodeid);

        // han add
        int _new_node = -1;
        void dumpLoad(int num_agents);
        void dumpTrans(int num_agents);
        void dumpDag();
        vector<vector<int>> evalColoringGlobal(vector<vector<int>> loadTable);
        void changeColor(int idx, int new_color);
        void changeColor2(int idx, int new_color);
        void dumpPlacement();

        vector<vector<int>> evaluateChange(int agent_num, int idx, int new_color);
        vector<vector<int>> evaluateChange2(int agent_num, vector<int> idx, int new_color);

        // int count_common_nodes(const Stripe& s1, const Stripe& s2);
        // vector<Stripe*> rearrange_stripes(vector<Stripe*> original_list) ;




};

#endif
