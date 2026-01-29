#include "CentSolution.hh"

CentSolution::CentSolution(){

}

CentSolution::CentSolution(int batchsize, int standbysize, int agentsnum) {
    _batch_size = batchsize;
    _standby_size = standbysize;
    _agents_num = agentsnum;
    _cluster_size = agentsnum+standbysize;
}

void CentSolution::genRepairBatches(int num_failures, vector<int> fail_node_list, int num_agents, string scenario, bool enqueue) {
    // We assume that the replacement node ids are the same with the failed ids
    cout << "CentSolution::genRepairBatches" << endl;

    if (num_failures == 1) {
        genRepairBatchesForSingleFailure(fail_node_list[0], num_agents, scenario, enqueue);
    }else{
        genRepairBatchesForMultipleFailure(fail_node_list, num_agents, scenario, enqueue);
    }

    _finish_gen_batches = true;
}

void CentSolution::genRepairBatchesForSingleFailure(int fail_node_id, int num_agents, string scenario, bool enqueue) {

    cout << "CentSolution::genRepairBatchesForSingleFailure.fail_node_id = " << fail_node_id << endl;

    // 0. we first figure out stripes that stores a block in $fail_node_id
    filterFailedStripes({fail_node_id});
    cout << "CentSolution::genRepairBatchesForSingleFailure.stripes to repair: " << _stripes_to_repair.size() << endl;

    // 1. we divide stripes to repair into batches of size $batchsize
    _num_batches = _stripes_to_repair.size() / _batch_size;
    if (_stripes_to_repair.size() % _batch_size != 0) {        
        _num_batches += 1; 
    }
    cout << "CentSolution::genRepairBatchesForSingleFailure.num batches = " << _num_batches << endl;

    for (int batchid=0; batchid<_num_batches; batchid++) {

        vector<Stripe*> cur_stripe_list;

        // i refers to the i-th stripe in this batch
        for (int i=0; i<_batch_size; i++) {
            // stripeidx refers to the idx in _stripes_to_repair
            int stripeidx = batchid * _batch_size + i;
            if (stripeidx < _stripes_to_repair.size()) {

                // stripeid refers to the actual id of stripe in all the stripes
                int stripeid = _stripes_to_repair[stripeidx];
                Stripe* curstripe = _stripe_list[stripeid];

                // 1.1 construct ECDAG to repair
                vector<int> tmp_fnid;
                tmp_fnid.push_back(fail_node_id);

                //ECDAG* curecdag = curstripe->genRepairECDAG(_ec, fail_node_id);
                ECDAG* curecdag = curstripe->genRepairECDAG(_ec, tmp_fnid);

                // 1.2 generate centralized coloring for the current stripe
                unordered_map<int, int> curcoloring;
                genCentralizedColoringForSingleFailure(curstripe, curcoloring, fail_node_id, num_agents, scenario);

                // 1.3 set the coloring result in curstripe
                curstripe->setColoring(curcoloring);

                // 1.4 evaluate the coloring solution
                curstripe->evaluateColoring();

                // 1.4 insert curstripe into cur_stripe_list
                cur_stripe_list.push_back(curstripe);
            }

        }

        // generate a batch based on current stripe list
        RepairBatch* curbatch = new RepairBatch(batchid, cur_stripe_list);
        curbatch->evaluateBatch(num_agents);

        // insert current batch into batch list or batch queue
        if (enqueue) {
            _batch_queue.push(curbatch);
        } else {
            _batch_list.push_back(curbatch);
        }
        curbatch->dump();

        //break;
    }

}


void CentSolution::genRepairBatchesForMultipleFailure(vector<int> fail_node_ids, int num_agents, string scenario, bool enqueue) {
    
    LOG << "CentSolution::genRepairBatchesForMultipleFailure begin" << endl;
    // 0. we first figure out stripes that stores a block in $fail_node_id
    filterFailedStripes(fail_node_ids);


    // 1. we divide stripes to repair into batches of size $batchsize
    _num_batches = _stripes_to_repair.size() / _batch_size;
    if (_stripes_to_repair.size() % _batch_size != 0) {        
        _num_batches += 1; 
    }
    LOG << "CentSolution::genRepairBatchesForMultipleFailure.num batches = " << _num_batches << endl;

    for (int batchid=0; batchid<_num_batches; batchid++) {

        vector<Stripe*> cur_stripe_list;

        // i refers to the i-th stripe in this batch
        for (int i=0; i<_batch_size; i++) {
            // stripeidx refers to the idx in _stripes_to_repair
            int stripeidx = batchid * _batch_size + i;
            if (stripeidx < _stripes_to_repair.size()) {

                // stripeid refers to the actual id of stripe in all the stripes
                int stripeid = _stripes_to_repair[stripeidx];
                Stripe* curstripe = _stripe_list[stripeid];

                // 1.1 construct ECDAG to repair
                
                ECDAG* ecdag = curstripe->genRepairECDAG(_ec, fail_node_ids);
          

                // 1.2 generate centralized coloring for the current stripe
                unordered_map<int, int> curcoloring;
                genCentralizedColoringForMutipleFailure(curstripe, curcoloring, fail_node_ids, num_agents, scenario);
                // 1.3 set the coloring result in curstripe
                curstripe->setColoring(curcoloring);
                // 1.4 evaluate the coloring solution
                curstripe->evaluateColoring();
                // 1.4 insert curstripe into cur_stripe_list
                cur_stripe_list.push_back(curstripe);
            }

        }
        
        // generate a batch based on current stripe list
        RepairBatch* curbatch = new RepairBatch(batchid, cur_stripe_list);
        curbatch->evaluateBatch(num_agents);

        // insert current batch into batch list or batch queue
        if (enqueue) {
            _batch_queue.push(curbatch);
        } else {
            _batch_list.push_back(curbatch);
        }
        //curbatch->dump();

        //break;
    }

}


void CentSolution::genCentralizedColoringForSingleFailure(Stripe* stripe, unordered_map<int, int>& res, 
        int fail_node_id, int num_agents, string scenario) {
    // map a sub-packet idx to a real physical node id
    
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    // 0. get leave vertices
    vector<int> leaves = ecdag->getECLeaves();
    cout << "leave num: " << leaves.size() << endl;
    cout << "  ";
    for (int i=0; i<leaves.size(); i++)
       cout << leaves[i] << " ";
    cout << endl;

    // 1. get all vertices
    vector<int> allvertices = ecdag->getAllNodeIds();
    cout << "all idx: " << allvertices.size() << endl;
    cout << "  ";
    for (int i=0; i<allvertices.size(); i++)
       cout << allvertices[i] << " ";
    cout << endl;

    // 2. figure out node id of leaves
    vector<int> avoid_node_ids;
    for (int i=0; i<leaves.size(); i++) {
        int dagidx = leaves[i];
        int blkidx = dagidx/ecw;

        int nodeid = -1;
        if (blkidx < ecn) {
            // it's a real block, otherwise, it's a virtual block(shortening)
            nodeid = curplacement[blkidx];
            avoid_node_ids.push_back(nodeid);
        }

        //cout << "dagidx: " << dagidx << ", blkidx: " << blkidx << ", nodeid: " << nodeid << endl;
        res.insert(make_pair(dagidx, nodeid));
    }
    // 2.1 avoid fail nodeid
    avoid_node_ids.push_back(fail_node_id);

    // 3. figure out a nodeid that performs the centralized repair
    int repair_node_id;
    if (scenario == "standby")
        repair_node_id = fail_node_id;
    else {
        // randomly choose a node
        vector<int> candidates;
        // 3.0 remove nodes that we should avoid
        for (int i=0; i<num_agents; i++) {
            if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                candidates.push_back(i);
        }
        
        // 3.1 randomly choose a node from candidates
        int tmpidx = rand() % candidates.size();
        repair_node_id = candidates[tmpidx];
    }

    // 4. for all the dagidx in allvertices, record nodeid in res
    for (int i=0; i<allvertices.size(); i++) {
        int dagidx = allvertices[i];
        if (res.find(dagidx) != res.end())
            continue;
        else {
            res.insert(make_pair(dagidx, repair_node_id));
        }
    }
}

void CentSolution::genCentralizedColoringForMutipleFailure(Stripe* stripe, unordered_map<int, int>& res, 
        vector<int> fail_node_ids, int num_agents, string scenario) {
    LOG << "CentSolution::genCentralizedColoringForSingleFailure start !"<<endl;
    // map a sub-packet idx to a real physical node id
    int failnum = stripe->getFailNum();
    // cout << "fail num = " << failnum << endl;
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    // 0. get leave vertices
    vector<int> leaves = ecdag->getECLeaves();
    // 1. get all vertices
    vector<int> allvertices = ecdag->getAllNodeIds();

    // 2. figure out node id of leaves
    vector<int> avoid_node_ids;
    for (int i=0; i<leaves.size(); i++) {
        int dagidx = leaves[i];
        int blkidx = dagidx/ecw;

        int nodeid = -1;
        if (blkidx < ecn) {
            // it's a real block, otherwise, it's a virtual block(shortening)
            nodeid = curplacement[blkidx];
            avoid_node_ids.push_back(nodeid);
        }
        res.insert(make_pair(dagidx, nodeid));
    }
    // 2.1 avoid fail nodeid
    avoid_node_ids.insert(avoid_node_ids.end(),fail_node_ids.begin(),fail_node_ids.end());


    // 3. figure out a nodeid that performs the centralized repair
    int repair_node_id;
    vector<int> ret;
    if (scenario == "standby")
        exit(1);
    else {
        // randomly choose a node
        vector<int> candidates;
        // 3.0 remove nodes that we should avoid
        for (int i=0; i<num_agents; i++) {
            if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                candidates.push_back(i);
        }
        assert(candidates.size() >= fail_node_ids.size());
        
        for(int i = 0; i < failnum; i++){
            int rand_idx = rand() % candidates.size();
            while(find(ret.begin(), ret.end(), rand_idx) != ret.end()){
                rand_idx = rand() % candidates.size();
            }
            ret.push_back(candidates[rand_idx]);
        }
    }

    int relayer_node = ret[0];


    // 4. for all the dagidx in allvertices, record nodeid in res
    for (int i=0; i<allvertices.size(); i++) {
        int dagidx = allvertices[i];
        if (res.find(dagidx) != res.end())
            continue;
        else {
            res.insert(make_pair(dagidx, relayer_node));
        }
    }
    vector<int> concact = stripe->getECDAG()->_ecConcacts;
    for(int i = 0; i < failnum; i++){
        int concact_vetex = concact[i];
        res[concact_vetex] = ret[i];
        cout << "concact_vetex: " << concact_vetex << ", color: " <<  ret[i] << endl;
    }

    LOG << "CentSolution::genCentralizedColoringForMutipleFailure end !"<<endl;

}
