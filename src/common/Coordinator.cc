#include "Coordinator.hh"

Coordinator::Coordinator(Config* conf, StripeStore* ss) {
    _conf = conf;
    _ss = ss;

    _codename = conf->_codeName;
    _ecn = conf->_ecn;
    _eck = conf->_eck;
    _ecw = conf->_ecw;
    _blkbytes = conf->_blkBytes;
    _pktbytes = conf->_pktBytes;
    _agents_num = conf->_agents_num;
    _standby_size = conf->_standby_size;
    _batch_size = conf->_batch_size;

    cout << "codename: " << _codename << endl;
    cout << "ecn: " << _ecn << endl;
    cout << "eck: " << _eck << endl;
    cout << "ecw: " << _ecw << endl;
    cout << "blkbytes: " << _blkbytes << endl;
    cout << "pktbytes: " << _pktbytes << endl;
}

Coordinator::~Coordinator() {

    // note that repair batches are generated inside coordinator
    // thus we free then in coordinator
    for (int i=0; i<_repair_batch_list.size(); i++) {
        RepairBatch* curbatch = _repair_batch_list[i];
        if (curbatch)
            delete curbatch;
    }

    if (_ec)
        delete _ec;
}

bool Coordinator::initRepair(string method, string scenario, int failnodeid) {

    _method = method;
    _scenario = scenario;
    _failnodeids = {failnodeid};
    vector<string> param;
    // 1. create ECBase

    if (_codename == "Clay") {
        _ec = new Clay(_ecn, _eck, _ecw, {to_string(_ecn-1)});
    } else if (_codename == "RDP") {
        _ec = new RDP(_ecn, _eck, _ecw, param);
    } else if (_codename == "HHXORPlus") {
        _ec = new HHXORPlus(_ecn, _eck, _ecw, param);
    } else if (_codename == "BUTTERFLY") {
        _ec = new BUTTERFLY(_ecn, _eck, _ecw, param);
    } else if (_codename == "RSCONV") {
        _ec = new RSCONV(_ecn, _eck, _ecw, param);
    } else if (_codename == "RSPIPE") {
        _ec = new RSPIPE(_ecn, _eck, _ecw, param);
    } else    {
        cout << "Non-supported code!" << endl;
        return 0;
    }
    int batchsize = _conf->_batch_size;
    int agentnum = _conf->_agents_num;
    int standbysize = _conf->_standby_size;

    // 2. create solutionk
    if (method == "centralize") {
        _sol = new CentSolution(batchsize,standbysize,agentnum);
        _sol->init(_ss->getStripeList(), _ec, _codename, _conf);
    } else if (method == "offline") {
        _sol = new OfflineSolution(batchsize,standbysize,agentnum);
        _sol->init(_ss->getStripeList(), _ec, _codename, _conf);
    } else if (method == "parallel") {
        _sol = new ParallelSolution(batchsize,standbysize,agentnum,0);
        _sol->init(_ss->getStripeList(), _ec, _codename, _conf);
    }

    // 3. fail2repair
    if (scenario == "standby") {
        _fail2repair.insert(make_pair(failnodeid, _conf->_agentsIPs.size()));
    }

    return true;
}

bool Coordinator::initRepair(string method, string scenario, vector<int> failnodeids) {

    _method = method;
    _scenario = scenario;
    _failnodeids = failnodeids;
    vector<string> param;
    // 1. create ECBase

    if (_codename == "Clay") {
        _ec = new Clay(_ecn, _eck, _ecw, {to_string(_ecn-1)});
    } else if (_codename == "RDP") {
        _ec = new RDP(_ecn, _eck, _ecw, param);
    } else if (_codename == "HHXORPlus") {
        _ec = new HHXORPlus(_ecn, _eck, _ecw, param);
    } else if (_codename == "BUTTERFLY") {
        _ec = new BUTTERFLY(_ecn, _eck, _ecw, param);
    } else if (_codename == "RSCONV") {
        _ec = new RSCONV(_ecn, _eck, _ecw, param);
    } else if (_codename == "RSPIPE") {
        _ec = new RSPIPE(_ecn, _eck, _ecw, param);
    } else    {
        cout << "Non-supported code!" << endl;
        return 0;
    }
    int batchsize = _conf->_batch_size;
    int agentnum = _conf->_agents_num;
    int standbysize = _conf->_standby_size;

    // 2. create solutionk
    if (method == "centralize") {
        _sol = new CentSolution(batchsize,standbysize,agentnum);
        _sol->init(_ss->getStripeList(), _ec, _codename, _conf);
    } else if (method == "offline") {
        _sol = new OfflineSolution(batchsize,standbysize,agentnum);
        _sol->init(_ss->getStripeList(), _ec, _codename, _conf);
    } else if (method == "parallel") {
        _sol = new ParallelSolution(batchsize,standbysize,agentnum,0);
        _sol->init(_ss->getStripeList(), _ec, _codename, _conf);
    }

    // 3. fail2repair
    if (scenario == "standby") {
        for(auto it : _failnodeids){
            _fail2repair.insert(make_pair(it, _conf->_agentsIPs.size()));
        }
    }

    return true;
}


vector<RepairBatch*> Coordinator::genRepairSolution() {
    
    // 2. generate repair batches
    _sol->genRepairBatches(1, _failnodeids, _conf->_agentsIPs.size(), _scenario, false);

    // 3. get repair batches
    _repair_batch_list = _sol->getRepairBatches();

    return _repair_batch_list;
}

int Coordinator::genRepairSolutionAsync() {
    // generate repair batches
    _sol->genRepairBatches(_failnodeids.size(), _failnodeids, _conf->_agentsIPs.size() + _conf ->_repairIPs.size(), _scenario, true);
    return 0;
}

void Coordinator::repair() {
    cout << "Coordinator::repair start" << endl; 
    struct timeval time1, time2, time3, time4, time5;
    double getbatchtime = 0, repairtime = 0, overall = 0;
    gettimeofday(&time1, NULL);
    unsigned int coorIp = _conf->_coorIp;
    int batchid = 0;
    vector<RepairBatch*> batch_list;
    vector<double>  gen_batch_vec;
    vector<double> repair_batch_vec;
    while (_sol->hasNext()) {
        gettimeofday(&time2, NULL);
        
	cout<<"Coordinator:: repair  getRepairBatchFromQueue start "<<endl;
	RepairBatch* curbatch = _sol->getRepairBatchFromQueue(); //卡住了
	
	
	int id_test = curbatch -> getBatchId();
	cout <<"cur batch id = "<<id_test<<endl;
	batch_list.push_back(curbatch);
        gettimeofday(&time3, NULL);
        batchid++;

        double t1 = DistUtil::duration(time2, time3);
        getbatchtime += t1;
        gen_batch_vec.push_back(t1);
        
	cout << "Coordinator:: repair  genRepairTasks start"<<endl;
	curbatch->genRepairTasks(_ecn, _eck, _ecw, _conf, _fail2repair, coorIp);
        gettimeofday(&time4, NULL);

        double t2 = DistUtil::duration(time3, time4);
        repairtime += t2;
        repair_batch_vec.push_back(t2);
        cout << "Coordinator::repair batch" << curbatch->getBatchId() << " get batch time = " << t1 << ", repair time = " << t2 << ", stripenum = " << curbatch->getStripeList().size() << endl;
    }
    gettimeofday(&time5, NULL);
    for(int i = 0; i < batch_list.size(); i++)
    {
        batch_list[i]->dump();

        cout<<"!!!!!!"<<endl;
        cout << "gen batch duration = " << gen_batch_vec[i] << endl;
        cout << "repair batch duration = " << repair_batch_vec[i] << endl;
        cout << "avg time = " << repair_batch_vec[i] / batch_list[i]->getStripeList().size()  << endl;
        delete batch_list[i];
    }
    
    overall = DistUtil::duration(time1, time5);
    LOG << "Coordinator::repair. genbatch  = " << getbatchtime << endl;
    LOG << "Coordinator::repair. repairbatch = " << repairtime << endl;
    cout << "Coordinator::repair. overall  = " << overall << endl;
}

