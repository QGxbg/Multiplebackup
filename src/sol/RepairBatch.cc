#include "RepairBatch.hh"

RepairBatch::RepairBatch(int id, vector<Stripe*> stripe_list) {

    _batch_id = id;
    _stripe_list = stripe_list;
}

RepairBatch::~RepairBatch() {

}

void RepairBatch::evaluateBatch(int num_agents) {
    _load = 0;
    _bdwt = 0;
    _in.clear();
    _out.clear();

    for (int i=0; i<_stripe_list.size(); i++) {
        Stripe* curstripe = _stripe_list[i];
        int stripeid = curstripe->getStripeId();

        unordered_map<int, int> stripe_in_map = curstripe->getInMap();
        unordered_map<int, int> stripe_out_map = curstripe->getOutMap();

        // LOG << "Stripe " << stripeid << endl;
        // LOG << "  inmap:  ";
        for (int nodeid=0; nodeid<num_agents; nodeid++) {
            int count = 0;
            if (stripe_in_map.find(nodeid) != stripe_in_map.end())
                count = stripe_in_map[nodeid];
            
            if (_in.find(nodeid) == _in.end()) {
                _in.insert(make_pair(nodeid, count));
            } else {
                _in[nodeid] += count;
            } 

            // LOG << count << " ";
        }
        // LOG << endl;

        // LOG << "  outmap: ";
        for (int nodeid=0; nodeid<num_agents; nodeid++) {
            int count = 0;
            if (stripe_out_map.find(nodeid) != stripe_out_map.end())
                count = stripe_out_map[nodeid];
            
            if (_out.find(nodeid) == _out.end()) {
                _out.insert(make_pair(nodeid, count));
            } else {
                _out[nodeid] += count;
            } 

            // LOG << count << " ";
        }
        // LOG << endl;
    }

    // LOG << "Batch " << _batch_id << endl;
    // LOG << "  inmap:  ";
    for (int nodeid=0; nodeid<num_agents; nodeid++) {
        int count=0;
        if (_in.find(nodeid) != _in.end())
            count = _in[nodeid];
        _bdwt += count;
        if (count > _load)
            _load = count;

        // LOG << count << " ";
    }
    // LOG << endl;
    // LOG << "  outmap: ";
    for (int nodeid=0; nodeid<num_agents; nodeid++) {
        int count=0;
        if (_out.find(nodeid) != _out.end())
            count = _out[nodeid];

        if (count > _load)
            _load = count;
        // LOG << count << " ";
    }
    // LOG << endl;
    // LOG << "  bitvec: ";
    // LOG << "load: " << _load << endl;
    // LOG << "    bdwt: " << _bdwt;
    // LOG << "    avg load :" << _load*1.0/_stripe_list.size() << endl;
}

vector<Stripe*> RepairBatch::getStripeList()
{
    return _stripe_list;
}

unordered_map<int,int>  RepairBatch::getInputMap()
{
    return _in;
}

unordered_map<int,int> RepairBatch::getOutputMap()
{
    return _out;
}

vector<vector<int>> RepairBatch::getLoadTable(int num_agents)
{
    vector<vector<int>> ret(num_agents, {0,0});
    for(auto stripe : _stripe_list)
    {
        for(auto it : stripe->getInMap())
        {
            ret[it.first][1] += it.second;
        }
        for(auto it : stripe->getOutMap())
        {
            ret[it.first][0] += it.second;
        }
    }
    return ret;
}

void RepairBatch::erase(Stripe* stripe)
{
    _stripe_list.erase(remove(_stripe_list.begin(), _stripe_list.end(), stripe), _stripe_list.end());
}

void RepairBatch::push(Stripe* stripe)
{
    _stripe_list.push_back(stripe);

    for(auto it : stripe->getInMap())
    {
        _in[it.first] += it.second;
    }
    for(auto it : stripe->getOutMap())
    {
        _out[it.first] += it.second;
    }
    
    _bdwt = 0;
    _load = 0;
    for(auto it: _in)
    {
        _bdwt += it.second;
        _load = max(_load, it.second);
    }
    for(auto it: _out)
    {
        _load = max(_load, it.second);
    }
}

Stripe* RepairBatch::pop()
{
    Stripe* ret = _stripe_list.back();
    _stripe_list.pop_back();
    return ret;
}

void RepairBatch::dumpLoad(int agent_num)
{
    LOG << "batch: " << _batch_id << endl;
    LOG << "   in :";
    for(int i = 0 ; i < agent_num; i ++)
    {
        LOG << _in[i] << " ";
    }
    LOG << endl;
    LOG << "   out:";
    for(int i = 0 ; i < agent_num; i ++)
    {
        LOG << _out[i] << " ";
    }
    LOG << endl;
}

int RepairBatch::getLoad() {
    return _load;
}

int RepairBatch::getBdwt() {
    return _bdwt;
}

void RepairBatch::dump() {
    LOG << "RepairBatch " << _batch_id << ": " << endl;
    for (int idx=0; idx<_stripe_list.size(); idx++) {
        Stripe* curstripe = _stripe_list[idx];
        int stripeid = curstripe->getStripeId();
        vector<int> curplace = curstripe->getPlacement();
        LOG << "    stripe " << stripeid <<  ": ";
        for (int i=0; i<curplace.size(); i++) {
            LOG << curplace[i] <<  " ";
        }
        LOG << "  newnode="  << curstripe->_new_node << endl;
    }
    LOG << "    stirpe num = "  << _stripe_list.size();
    LOG << "    load="<< _load;
    LOG << "    avgload = " << _load*1.0/ _stripe_list.size() << endl << endl;
}

void RepairBatch::dump(int num_agents) {
    LOG << "RepairBatch " << _batch_id << ": " << endl;
    for (int idx=0; idx<_stripe_list.size(); idx++) {
        Stripe* curstripe = _stripe_list[idx];
        int stripeid = curstripe->getStripeId();
        vector<int> curplace = curstripe->getPlacement();
        LOG << "    stripe " << stripeid <<  ": ";
        for (int i=0; i<curplace.size(); i++) {
            LOG << curplace[i] <<  " ";
        }
        LOG << endl;
    }
    LOG << "    stirpe num = "  << _stripe_list.size();
    LOG << "    load="<< _load;
    LOG << "    avgload = " << _load*1.0/ _stripe_list.size() << endl << endl;
    dumpLoad(num_agents);
}

void RepairBatch::genRepairTasks(int ecn, int eck, int ecw, Config* conf, 
        unordered_map<int, int> fail2repair, unsigned int coorIp) {

    struct timeval time1, time2, time3, time4, time5, time6;
    gettimeofday(&time1, NULL);

    // 0. generate tasks for each stripe
    unordered_map<int, vector<int>> nodeid2internalidx;

    for (int i=0; i<_stripe_list.size(); i++) {
        Stripe* curstripe = _stripe_list[i];
        unordered_map<int, vector<Task*>> curtaskmap = curstripe->genRepairTasks(_batch_id, ecn, eck, ecw, fail2repair);
        
        // mark that nodeid needs to deal with tasks for the i-th stripe in this batch
        for (auto item: curtaskmap) {
            int nodeid = item.first;
            if (nodeid2internalidx.find(nodeid) == nodeid2internalidx.end()) {
                vector<int> list = {i};
                nodeid2internalidx.insert(make_pair(nodeid, list));
            } else
                nodeid2internalidx[nodeid].push_back(i);
        }

        //break;
    }
    gettimeofday(&time2, NULL);
    LOG << "RepairBatch::genRepairTasks.time1-time2.sort task time: " << DistUtil::duration(time1, time2) << endl;

    // 1. for each node, send command to agent to tell the number of stripes that the agent need to deal with
    unordered_map<int, NodeBatchTask*> batchtaskmap;
    for (auto item: nodeid2internalidx) {
        int nodeid = item.first;
        vector<int> internalidx = item.second;

        // 1.0 nodeid needs to deal with tasks from $nstripes stripes
        int nstripes = internalidx.size();
        vector<int> stripelist;
        vector<int> numlist;
        unordered_map<int, vector<Task*>> stripetaskmap;

        for (int i=0; i<internalidx.size(); i++) {
            int ii = internalidx[i];
            Stripe* curstripe = _stripe_list[ii];
            int stripeid = curstripe->getStripeId();
            int tasknum = curstripe->getTaskNumForNodeId(nodeid);
            vector<Task*> tasklist = curstripe->getTaskForNodeId(nodeid);
            
            for(auto it : tasklist)
            {
                it->dump();
            }

            stripelist.push_back(stripeid);
            numlist.push_back(tasknum);
            stripetaskmap.insert(make_pair(stripeid, tasklist));
        }

        // 1.1 get ip for nodeid
        unsigned int ip;
        if (nodeid < conf->_agentsIPs.size()) {
            // nodeid is an agent
            ip = conf->_agentsIPs[nodeid];
        } else {
            // nodeid is a repair node
            int idx = nodeid - conf->_agentsIPs.size();
            ip = conf->_repairIPs[idx];
        }

        // 1.2 prepare NodeBatchTask
        NodeBatchTask* nbtask = new NodeBatchTask(_batch_id, stripelist, numlist, stripetaskmap, ip);

        //// 1.3 send AGCommand
        //nbtask->sendAGCommand(ip);
        //LOG << "RepairBatch::send to " << RedisUtil::ip2Str(ip) << endl;

        //// 1.4 send TaskCommands
        //nbtask->sendTaskCommands(ip);

        // 1.5 add nbtask into map
        batchtaskmap.insert(make_pair(nodeid, nbtask));
    }
    gettimeofday(&time3, NULL);
    LOG << "RepairBatch::genRepairTasks.time2-time3.generate tasks for nodes: " << DistUtil::duration(time2, time3) << endl;

    int threadnum = batchtaskmap.size();
    thread thrds[threadnum];
    int tid=0;
    for (auto item: batchtaskmap) { 
        int nodeid = item.first;
        NodeBatchTask* nbtask = item.second;
        thrds[tid++] = thread([=]{nbtask->sendCommands();});
    }

    for (int i=0; i<tid; i++) { 
        thrds[i].join();
    }
    gettimeofday(&time4, NULL);
    LOG << "RepairBatch::genRepairTasks.time3-time4.send tasks for nodes: " << DistUtil::duration(time3, time4) << endl;
    LOG << "RepairBatch::genRepairTasks.node2internalidx.size = " << nodeid2internalidx.size() << endl;
    LOG << "RepairBatch::genRepairTasks.batchtaskmap.size = " << batchtaskmap.size() << endl;

    // 2. for each node, we wait for a response that make sure corresponding node finishes assigned tasks for this batch
    tid=0;
    for (auto item: nodeid2internalidx) {
        int nodeid = item.first;
        NodeBatchTask* nbtask = batchtaskmap[nodeid];
        cout<<"326hang nodeid:"<<nodeid<<endl;
        thrds[tid++] = thread([=]{nbtask->waitFinishFlag(nodeid, coorIp);});
    }
    for (int i=0; i<tid; i++) {
        thrds[i].join();
    }
    gettimeofday(&time5, NULL);
    LOG << "RepairBatch::genRepairTasks.time4-time5.wait finish flag: " << DistUtil::duration(time4, time5) << endl;
    cout << "RepairBatch::genRepairTasks finish " << endl;

    // 3. clean
    for (auto item: batchtaskmap) {
        NodeBatchTask* nbtask = item.second;
        if (nbtask)
            delete nbtask;
    }
    gettimeofday(&time6, NULL);
    LOG << "RepairBatch::genRepairTasks.time5-time6.clean: " << DistUtil::duration(time5, time6) << endl;
}

int RepairBatch::getBatchId()
{
    return _batch_id;
}

