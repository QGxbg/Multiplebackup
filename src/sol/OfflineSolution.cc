#include "OfflineSolution.hh"

OfflineSolution::OfflineSolution() {}

OfflineSolution::OfflineSolution(int batchsize, int standbysize,
                                 int agentsnum) {
  _batch_size = batchsize;
  _standby_size = standbysize;
  _agents_num = agentsnum;
  _cluster_size = agentsnum + standbysize;
}

void OfflineSolution::genRepairBatches(int num_failures,
                                       vector<int> fail_node_list,
                                       int num_agents, string scenario,
                                       bool enqueue, int batch_method) {
  // We assume that the replacement node ids are the same with the failed ids
  // cout << "OfflineSolution::genRepairBatches" << endl;

  // read offline tradeoff points
  int ecn = _ec->_n;
  int eck = _ec->_k;
  int ecw = _ec->_w;
  string offline_solution_path = _conf->_tpDir + "/" + _codename + "_" +
                                 to_string(ecn) + "_" + to_string(eck) + "_" +
                                 to_string(ecw) + ".xml";
  // cout << "offline solution path: " << offline_solution_path << endl;
  _tp = new TradeoffPoints(offline_solution_path);

  if (num_failures == 1) {
    genRepairBatchesForSingleFailure(fail_node_list[0], num_agents, scenario,
                                     enqueue);
  } else {
    genRepairBatchesForMuitipleFailure(fail_node_list, num_agents, scenario,
                                       enqueue);
  }

  _finish_gen_batches = true;
}

void OfflineSolution::genRepairBatchesForSingleFailure(int fail_node_id,
                                                       int num_agents,
                                                       string scenario,
                                                       bool enqueue) {
  LOG << "OfflineSolution::genRepairBatchesForSingleFailure start" << endl;
  LOG << "fail_node_id = " << fail_node_id << endl;

  // 0. we first figure out stripes that stores a block in $fail_node_id
  filterFailedStripes({fail_node_id});
  LOG << "stripes to repair: " << _stripes_to_repair.size() << endl;

  // 1. we divide stripes to repair into batches of size $batchsize
  _num_batches = _stripes_to_repair.size() / _batch_size;
  if (_stripes_to_repair.size() % _batch_size != 0) {
    _num_batches += 1;
  }
  LOG << "num batches = " << _num_batches << endl;

  for (int batchid = 0; batchid < _num_batches; batchid++) {

    vector<Stripe *> cur_stripe_list;

    // i refers to the i-th stripe in this batch
    for (int i = 0; i < _batch_size; i++) {
      // stripeidx refers to the idx in _stripes_to_repair
      int stripeidx = batchid * _batch_size + i;
      if (stripeidx < _stripes_to_repair.size()) {

        // stripeid refers to the actual id of stripe in all the stripes
        int stripeid = _stripes_to_repair[stripeidx];
        Stripe *curstripe = _stripe_list[stripeid];

        // 1.1 construct ECDAG to repair
        vector<int> tmp_fnid;
        tmp_fnid.push_back(fail_node_id);
        ECDAG *curecdag = curstripe->genRepairECDAG(_ec, tmp_fnid);
        // ECDAG* curecdag = curstripe->genRepairECDAG(_ec, fail_node_id);

        // 1.2 generate offline coloring for the current stripe
        unordered_map<int, int> curcoloring;
        genOfflineColoringForSingleFailure(curstripe, curcoloring, fail_node_id,
                                           num_agents, scenario);

        // 1.3 set the coloring result in curstripe
        curstripe->setColoring(curcoloring);

        // 1.4 evaluate the coloring solution
        curstripe->evaluateColoring();

        // 1.4 insert curstripe into cur_stripe_list
        cur_stripe_list.push_back(curstripe);
      }
    }

    // generate a batch based on current stripe list
    RepairBatch *curbatch = new RepairBatch(batchid, cur_stripe_list);
    curbatch->evaluateBatch(num_agents);

    // insert current batch into batch list
    if (enqueue) {
      _batch_queue.push(curbatch);
    } else {
      _batch_list.push_back(curbatch);
    }
    curbatch->dump();
  }
  LOG << "OfflineSolution::genRepairBatchesForSingleFailure end" << endl;
}

void OfflineSolution::genOfflineColoringForSingleFailure(
    Stripe *stripe, unordered_map<int, int> &res, int fail_node_id,
    int num_agents, string scenario) {
  LOG << "OfflineSolution::genOfflineColoringForSingleFailure start" << endl;
  // cout << "OfflineSolution::genOfflineColoringForSingleFailure start" <<
  // endl; LOG << "fail_node_id:"<<fail_node_id<<endl;
  //  map a sub-packet idx to a real physical node id

  ECDAG *ecdag = stripe->getECDAG();
  vector<int> curplacement = stripe->getPlacement();

  // for(int i=0;i<curplacement.size();i++){
  //     cout<<curplacement[i]<<" ";
  // }
  // cout<<endl;

  int ecn = _ec->_n;
  int eck = _ec->_k;
  int ecw = _ec->_w;

  int fail_block_idx = -1;
  for (int i = 0; i < curplacement.size(); i++) {
    if (curplacement[i] == fail_node_id)
      fail_block_idx = i;
  }

  // 0. get data structures of ecdag
  unordered_map<int, ECNode *> ecNodeMap = ecdag->getECNodeMap();
  vector<int> ecHeaders = ecdag->getECHeaders();
  vector<int> ecLeaves = ecdag->getECLeaves();

  int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
  LOG << "number of intermediate vertices: " << intermediate_num << endl;

  // 1. color leave vertices
  // If a leave vertex is part of a real block, we first figure out its block
  // index, and then find corresponding node id If a leave vertex is part of a
  // virtual block (shortening), we mark nodeid as -1

  int realLeaves = 0;
  vector<int> avoid_node_ids;
  for (auto dagidx : ecLeaves) {
    int blkidx = dagidx / ecw;
    int nodeid = -1;

    if (blkidx < ecn) {
      nodeid = curplacement[blkidx];
      avoid_node_ids.push_back(nodeid);
      realLeaves++;
    }
    res.insert(make_pair(dagidx, nodeid));
  }
  avoid_node_ids.push_back(fail_node_id);

  cout << "avoid_node_ids:";
  for (int i = 0; i < avoid_node_ids.size(); i++) {
    cout << avoid_node_ids[i] << " ";
  }
  cout << endl;

  // 2. color header
  int repair_node_id;
  if (scenario == "standby")
    repair_node_id = fail_node_id;
  else {
    // randomly choose a node
    vector<int> candidates;
    // 2.0 remove nodes that we should avoid
    for (int i = 0; i < num_agents; i++) {
      if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) ==
          avoid_node_ids.end())
        candidates.push_back(i);
    }
    LOG << "num_agents:" << num_agents << endl;
    LOG << "candiates :" << candidates.size() << endl; // 0

    // 2.1 randomly choose a node from candidates
    int tmpidx = rand() % candidates.size();
    repair_node_id = candidates[tmpidx];
  }
  stripe->_new_node = repair_node_id;
  res.insert(make_pair(ecHeaders[0], repair_node_id));

  // 3. read from the offline solution file for coloring of intermediate nodes
  vector<int> itm_idx;
  for (auto item : ecNodeMap) {
    int dagidx = item.first;
    if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
      continue;
    if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
      continue;
    itm_idx.push_back(dagidx);
  }
  sort(itm_idx.begin(), itm_idx.end());

  // note that in offline solution, colors are within ecn
  // if color == fail_block_idx, find corresponding repair node as the real
  // color otherwise, color = block idx, find corresponding node id
  vector<int> itm_offline_coloring = _tp->getColoringByIdx(fail_block_idx);
  LOG << "itm_idx.size: " << itm_idx.size()
      << ", itm_offline_coloring.size: " << itm_offline_coloring.size() << endl;

  for (int i = 0; i < itm_idx.size(); i++) {
    int dagidx = itm_idx[i];
    int blkidx = itm_offline_coloring[i];
    LOG << "dagidx: " << dagidx << ", blkidx: " << blkidx << endl;

    int nodeid = -1;
    if (blkidx == fail_block_idx)
      nodeid = repair_node_id;
    else
      nodeid = curplacement[blkidx];

    res.insert(make_pair(dagidx, nodeid));
  }
  LOG << "OfflineSolution::genOfflineColoringForSingleFailure end" << endl;
}

void OfflineSolution::genRepairBatchesForMuitipleFailure(
    vector<int> fail_node_id, int num_agents, string scenario, bool enqueue) {
  LOG << "OfflineSolution::genRepairBatchesForMuitipleFailure start" << endl;
  LOG << "fail_node_id = ";
  for (int i = 0; i < fail_node_id.size(); i++) {
    LOG << fail_node_id[i] << " ";
  }
  LOG << endl;

  // 0. we first figure out stripes that stores a block in $fail_node_id
  filterFailedStripes(fail_node_id);
  LOG << "stripes to repair: " << _stripes_to_repair.size() << endl;

  // 1. we divide stripes to repair into batches of size $batchsize
  _num_batches = _stripes_to_repair.size() / _batch_size;
  if (_stripes_to_repair.size() % _batch_size != 0) {
    _num_batches += 1;
  }
  LOG << "num batches = " << _num_batches << endl;

  for (int batchid = 0; batchid < _num_batches; batchid++) {

    vector<Stripe *> cur_stripe_list;

    // i refers to the i-th stripe in this batch
    for (int i = 0; i < _batch_size; i++) {
      // stripeidx refers to the idx in _stripes_to_repair
      int stripeidx = batchid * _batch_size + i;
      if (stripeidx < _stripes_to_repair.size()) {

        // stripeid refers to the actual id of stripe in all the stripes
        int stripeid = _stripes_to_repair[stripeidx];
        Stripe *curstripe = _stripe_list[stripeid];

        // 1.1 construct ECDAG to repair
        vector<int> tmp_fnid;
        // tmp_fnid.push_back(fail_node_id);
        tmp_fnid.insert(tmp_fnid.end(), fail_node_id.begin(),
                        fail_node_id.end());
        ECDAG *curecdag = curstripe->genRepairECDAG(_ec, tmp_fnid);
        // ECDAG* curecdag = curstripe->genRepairECDAG(_ec, fail_node_id);

        // 1.2 generate offline coloring for the current stripe
        unordered_map<int, int> curcoloring;
        genOfflineColoringForMultipleFailure(
            curstripe, curcoloring, fail_node_id, num_agents, scenario);

        // 1.3 set the coloring result in curstripe
        curstripe->setColoring(curcoloring);

        // 1.4 evaluate the coloring solution
        curstripe->evaluateColoring();

        // 1.4 insert curstripe into cur_stripe_list
        cur_stripe_list.push_back(curstripe);
      }
    }

    // generate a batch based on current stripe list
    RepairBatch *curbatch = new RepairBatch(batchid, cur_stripe_list);
    curbatch->evaluateBatch(num_agents);

    // insert current batch into batch list
    if (enqueue) {
      _batch_queue.push(curbatch);
    } else {
      _batch_list.push_back(curbatch);
    }
    curbatch->dump();
  }
  LOG << "OfflineSolution::genRepairBatchesForMuitipleFailure end" << endl;
}

void OfflineSolution::genOfflineColoringForMultipleFailure(
    Stripe *stripe, unordered_map<int, int> &res, vector<int> fail_node_ids,
    int num_agents, string scenario) {
  LOG << "OfflineSolution::genOfflineColoringForMultipleFailure start" << endl;
  // cout << "OfflineSolution::genOfflineColoringForMultipleFailure start" <<
  //endl; 
  LOG << "fail_node_ids:";
  for (int i = 0; i < fail_node_ids.size(); i++) {
    LOG << fail_node_ids[i] << " ";
  }
  LOG << endl;

  //  map a sub-packet idx to a real physical node id

  ECDAG *ecdag = stripe->getECDAG();
  vector<int> curplacement = stripe->getPlacement();

  cout<<"curplacement:";  //0 3 1 2
  for(int i=0;i<curplacement.size();i++){
      cout<<curplacement[i]<<" ";
  }
  cout<<endl;

  int ecn = _ec->_n;
  int eck = _ec->_k;
  int ecw = _ec->_w;

  vector<int> fail_block_idx;
  for (int i = 0; i < curplacement.size(); i++) {
    for (auto fail_node_id : fail_node_ids) {
      if (curplacement[i] == fail_node_id)
        fail_block_idx.push_back(i);
    }
  }

  cout << "fail_block_idx:";  // 0 2
  for (int i = 0; i < fail_block_idx.size(); i++) {
    cout << fail_block_idx[i] << " ";
  }
  cout << endl;

  // 0. get data structures of ecdag
  unordered_map<int, ECNode *> ecNodeMap = ecdag->getECNodeMap();
  vector<int> ecHeaders = ecdag->getECHeaders();
  vector<int> ecLeaves = ecdag->getECLeaves();

  int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
  LOG << "number of intermediate vertices: " << intermediate_num << endl;

  // 1. color leave vertices
  // If a leave vertex is part of a real block, we first figure out its block
  // index, and then find corresponding node id If a leave vertex is part of a
  // virtual block (shortening), we mark nodeid as -1

  int realLeaves = 0;
  vector<int> avoid_node_ids = fail_node_ids;

  for (auto dagidx : ecLeaves) {
    int blkidx = dagidx / ecw;
    int nodeid = -1;
    if (blkidx < ecn) {
      nodeid = curplacement[blkidx];
      if (find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) ==
          avoid_node_ids.end())
        avoid_node_ids.push_back(nodeid);
      realLeaves++;
    }
    res.insert(make_pair(dagidx, nodeid));
  }

  cout << "avoid_node_ids:";
  for (int i = 0; i < avoid_node_ids.size(); i++) {
    cout << avoid_node_ids[i] << " ";
  }
  cout << endl;

  // 2. color header
  int repair_node_id;
  vector<int> candidates;
  if (scenario == "standby")
    // repair_node_id = fail_node_id;
    exit(1);
  else {
    for (auto concact_idx : ecdag->_ecConcacts) {
      // for each concact idx
      // remove source nodes that we should avoid
      candidates.clear();
      for (int i = 0; i < num_agents; i++) {
        if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) ==
            avoid_node_ids.end())
          candidates.push_back(i);
      }
      // random choose the replacement node for failblock
      repair_node_id = candidates[rand() % candidates.size()];
      avoid_node_ids.push_back(repair_node_id);
      res.insert(make_pair(concact_idx, repair_node_id));
      LOG << "DEBUG concact " << concact_idx << " choose " << repair_node_id
          << endl;
    }
  }
  // stripe->_new_node = repair_node_id;
  // res.insert(make_pair(ecHeaders[0], repair_node_id));

  // 3. read from the offline solution file for coloring of intermediate nodes
  vector<int> itm_idx;
  for (auto item : ecNodeMap) {
    int dagidx = item.first;
    if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
      continue;
    if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
      continue;
    itm_idx.push_back(dagidx);
  }
  sort(itm_idx.begin(), itm_idx.end());
  cout << itm_idx.size() << endl;

  // note that in offline solution, colors are within ecn
  // if color == fail_block_idx, find corresponding repair node as the real
  // color otherwise, color = block idx, find corresponding node id

  // vetcor<int> 转换为 int
  int fail_block_idx_int = 0;

  for (int i = 0; i < fail_block_idx.size(); i++) {
    fail_block_idx_int += pow(2, curplacement[fail_block_idx[i]]);   // 0 and 2
  }
  cout << fail_block_idx_int << endl;

  vector<int> itm_offline_coloring = _tp->getColoringByIdx(fail_block_idx_int);
  LOG << "itm_idx.size: " << itm_idx.size()
      << ", itm_offline_coloring.size: " << itm_offline_coloring.size() << endl;

  for (int i = 0; i < itm_idx.size(); i++) {
    int dagidx = itm_idx[i];
    int blkidx = itm_offline_coloring[i];
    LOG << "dagidx: " << dagidx << ", blkidx: " << blkidx << endl;

    // int nodeid = -1;
    //  if (blkidx == fail_block_idx)   //fail_block_idx  vector<int>
    //      nodeid = repair_node_id;
    //  else
    //      nodeid = curplacement[blkidx];

    // res.insert(make_pair(dagidx, nodeid));
    res.insert(make_pair(dagidx, blkidx));
  }
  LOG << "OfflineSolution::genRepairBatchesForMuitipleFailure end" << endl;
}

void OfflineSolution::setTradeoffPoints(TradeoffPoints *tp) { _tp = tp; }
