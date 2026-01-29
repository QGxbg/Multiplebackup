#include "Worker.hh"

Worker::Worker(Config* conf) {
    _conf = conf;
    _processCtx = RedisUtil::createContext(_conf -> _localIp);
    _batchtasklist = new BlockingQueue<NodeBatchTask*>();
}

Worker::~Worker() {
    delete _batchtasklist;
    redisFree(_processCtx);
}

void Worker::doProcess() {
    while (true) {
        // 0. read batchtasks from controller
        NodeBatchTask* nbtask = readInstructions();

        if (nbtask == NULL) {
            LOG << "ERROR::doProcess get NULL nbtask" << endl;
            sleep(1);
            continue;
        }

        // 1. repair batch
        repairBatch(nbtask);

        // 2. clean
        if (nbtask)
            delete nbtask;
    }
}

NodeBatchTask* Worker::readInstructions() {
    NodeBatchTask* nbtask = NULL;
    redisReply* rReply;
    LOG << "Worker::readInstructions " << endl;
    //cout<<"38  hang"<<endl;
    rReply = (redisReply*)redisCommand(_processCtx, "blpop ag_request 0");
    //cout<<"40 hang"<<endl;
    if (rReply == nullptr) {
        // 处理错误情况，可能是命令执行失败或者内存分配失败
        // 输出错误日志，进行适当的处理
        LOG << "Error executing redisCommand: " << _processCtx->errstr << std::endl;
        LOG << "server ip :" <<  RedisUtil::ip2Str(_conf->_localIp) << endl;
    } else if (rReply -> type == REDIS_REPLY_NIL) {
        LOG << "Worker::readInstructions get feed back empty queue " << endl;
    } else if (rReply -> type == REDIS_REPLY_ERROR) {
        LOG << "Worker::readInstructions get feed back ERROR happens " << endl;
    } else {
        struct timeval time1, time2;
        gettimeofday(&time1, NULL);
        if((int)rReply->elements == 0) {
            cerr << __func__ << " rReply->elements = 0, ERROR " << endl;
        }
        char* reqStr = rReply -> element[1] -> str;
        AGCommand* agCmd = new AGCommand(reqStr);
        
        int batchid = agCmd->getBatchId();
        int stripenum = agCmd->getNumStripes();
        vector<int> stripeidlist = agCmd->getStripeIdList();
        vector<int> stripetasknum = agCmd->getStripeTaskNum();
        
        LOG << "Worker::raedInstructions.batchid: " << batchid << ", stripenum: " << stripenum << endl;
        for (int i=0; i<stripenum; i++) {
            LOG << "    stripeid: " << stripeidlist[i] << ", tasknum: " << stripetasknum[i] << endl;
        }
        
        nbtask = recvBatchTasks(batchid, stripenum, stripeidlist, stripetasknum);
        
        delete agCmd;
    }
    freeReplyObject(rReply);

    return nbtask;
}

NodeBatchTask* Worker::recvBatchTasks(int batchid, int stripenum, vector<int> stripeidlist, vector<int> stripetasknum) {
    LOG << "Worker::recvBatchTasks batchid: " << batchid << ", stripenum: " << stripenum << endl;
   
    struct timeval time1, time2, time3, time4, time5, time6;
    gettimeofday(&time1, NULL);

    redisContext* recvCtx = RedisUtil::createContext(_conf -> _localIp);

    unordered_map<int, vector<Task*>> taskmap;

    for (int i=0; i<stripenum; i++) {
        int stripeid = stripeidlist[i];
        int tasknum = stripetasknum[i];

        vector<Task*> tasklist;
        string rkey = to_string(batchid)+":"+to_string(stripeid)+":command";
        
        for (int taskid=0; taskid<tasknum; taskid++) {
            redisReply* rReply;
            rReply = (redisReply*)redisCommand(recvCtx, "blpop %s 0", rkey.c_str());

            char* reqStr = rReply -> element[1] -> str;
            Task* curtask = new Task(reqStr, batchid, stripeid, _conf->_localIp);
            // curtask->dump();
            tasklist.push_back(curtask);

            freeReplyObject(rReply);
        }

        taskmap.insert(make_pair(stripeid, tasklist));
    }

    NodeBatchTask* nbtask = new NodeBatchTask(batchid, stripeidlist, stripetasknum, taskmap, 0);
    //_batchtasklist->push(nbtask);

    redisFree(recvCtx); 

    gettimeofday(&time2, NULL);
    LOG << "Worker::recvBatchTasks duration = " << DistUtil::duration(time1, time2) << endl;
    return nbtask;
}

/* parallel stripe */
void Worker::repairBatch(NodeBatchTask* nbtask) {

    struct timeval time1, time2, time3, time4, time5, time6;
    gettimeofday(&time1, NULL);

    // 0. get batch information
    int batchid = nbtask->_batch_id;
    vector<int> stripeidlist = nbtask->_stripe_id_list;
    vector<int> tasknumlist = nbtask->_num_list;
    unordered_map<int, vector<Task*>> taskmap = nbtask->_taskmap;

    LOG << "Start to repair batch " << batchid << ", at " << getTimeString(time1) << endl;

    // 1. create blocking queue for stripes in this batch
    for (int i=0; i<stripeidlist.size(); i++) {
        int stripeid = stripeidlist[i];
        DataMap* curmap = new DataMap();

        _lock.lock();
        _stripe2datamap.insert(make_pair(stripeid, curmap));
        _lock.unlock();

        vector<Task*> tasklist = taskmap[stripeid];
        for (Task* curtask: tasklist) {
            prepareDataMap(curtask, curmap);
        }
    }
    gettimeofday(&time2, NULL);

    // 2. call repair stripe
    thread thrds[stripeidlist.size()];
    int tid=0;
    LOG << "stripe num = " << stripeidlist.size() << ":" << endl;
    for(auto it : stripeidlist){
        LOG << it << " ";
    }
    LOG << endl;
    for (int i=0; i<stripeidlist.size(); i++) {

        int stripeid = stripeidlist[i];
        int tasknum = tasknumlist[i];
        vector<Task*> tasklist = taskmap[stripeid];
        
        thrds[tid++] = thread([=]{repairStripe(batchid, stripeid, tasknum, tasklist);});
    }

    for(int i=0; i<tid; i++)
        thrds[i].join();
    
    gettimeofday(&time3, NULL);
    LOG << " Done repair batch " << batchid << ", at " << getTimeString(time3) << endl; 
    // 3. delete blocking queue
    for (int i=0; i<stripeidlist.size(); i++) {
        int stripeid = stripeidlist[i];
        DataMap* curmap = _stripe2datamap[stripeid];
        if (curmap)
            delete curmap;
    }
    _stripe2datamap.clear();
    gettimeofday(&time4, NULL);

    sendFinishFlag(batchid);
    gettimeofday(&time5, NULL);

    LOG << "LOG::repair.createqueue: " << DistUtil::duration(time1, time2) << endl;
    LOG << "LOG::repair.runtime: " << DistUtil::duration(time2, time3) << endl;
    LOG << "LOG::repair.deletequeue: " << DistUtil::duration(time3, time4) << endl;
    LOG << "LOG::repair.sendflag: " << DistUtil::duration(time4, time5) << endl;
    LOG << "LOG::repair.overall: " << DistUtil::duration(time1, time5) << endl;
}

/* This is a sequential version */
//void Worker::repairBatch(NodeBatchTask* nbtask) {
//
//    cout << "--------repair batch--------" << endl;
//    struct timeval time1, time2, time3, time4, time5, time6;
//    gettimeofday(&time1, NULL);
//
//    // 0. get batch information
//    int batchid = nbtask->_batch_id;
//    vector<int> stripeidlist = nbtask->_stripe_id_list;
//    vector<int> tasknumlist = nbtask->_num_list;
//    unordered_map<int, vector<Task*>> taskmap = nbtask->_taskmap;
//    
//    cout << "-------- start to repair batch " << batchid << ", ts: " << getTimeStamp(time1) <<" --------" << endl;
//    
//    cout << "stripeidlist.size = " << stripeidlist.size() << endl;
//    // 1. each time we deal with tasks from a stripe
//    for (int i=0; i<stripeidlist.size(); i++) {
//        gettimeofday(&time2, NULL);
//
//        int stripeid = stripeidlist[i];
//        int tasknum = tasknumlist[i];
//        vector<Task*> tasklist = taskmap[stripeid];
//        
//        _sendNum = 0;
//        _recvNum = 0;
//        _computeNum = 0;
//
//
//        // 2. prepare mutex and conditional variables for dagidx
//        for (int taskid=0; taskid<tasklist.size(); taskid++) {
//            Task* curtask = tasklist[taskid];
//            prepareBQ(curtask);
//        }
//        
//        repairStripe(batchid, stripeid, tasknum, tasklist);
//        gettimeofday(&time3, NULL);
//
//        // 2. clean cached data structures in datamap
//        for (auto item: _datamap) {
//            int dagidx = item.first;
//            BlockingQueue<DataPacket*>* queue = item.second;
//
//            assert(queue->getSize() == 0);
//            if (queue)
//                delete queue;
//        }
//        _datamap.clear();
//        gettimeofday(&time4, NULL);
//        cout << "Worker::repairBatch. repair stripeid " << stripeid << ": repair time = "  << DistUtil::duration(time2, time3) << ", clear time = " << DistUtil::duration(time3, time4) << endl;
//    }
//    gettimeofday(&time5, NULL);
//    
//    // TODO: sync with coordinator for a next batch
//    sendFinishFlag(batchid);
//    
//    gettimeofday(&time6, NULL);
//    cout << "                                             repair.duration: " << DistUtil::duration(time1, time5) << ", send finish flag: " << DistUtil::duration(time5, time6)  << endl;
//}

void Worker::prepareBQ(Task* task) {
    int type = task->_type;
    if (type == 0) {
        // read
        string blk_name = task->_blk_name;
        vector<int> dagidx_list = task->_dagidx_list;
        vector<pair<bool, bool>> usage_list = task->_usage_list;

        if (blk_name == "ZERO") {
            // only generate for local blocking queue
            for (int dagidx: dagidx_list) {
                BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
                _lock.lock();
                _datamap.insert(make_pair(dagidx, queue));
                _lock.unlock();
            }
        } else {
            // generate based on usage
            for (int i=0; i<dagidx_list.size(); i++) {
                int dagidx = dagidx_list[i];
                pair<bool, bool> usage = usage_list[i];

                bool same = usage.first;
                bool diff = usage.second;

                if (same) {
                    BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
                    _lock.lock();
                    _datamap.insert(make_pair(dagidx, queue));
                    _lock.unlock();
                }

                if (diff) {
                    int target = -1 * dagidx;
                    if (dagidx == 0)
                        target = KEY0;

                    BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
                    _lock.lock();
                    _datamap.insert(make_pair(target, queue));
                    _lock.unlock();
                }
            }
        }
    } else if (type == 1) {
        // send: we have generated these queues
        //vector<int> dagidx_list;
    } else if (type == 2) {
        // recv
        vector<int> dagidx_list = task->_dagidx_list;
        for (auto dagidx: dagidx_list) {
            BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
            _lock.lock();
            _datamap.insert(make_pair(dagidx, queue));
            _lock.unlock();
        }
    } else if (type == 3) {
        // compute
        vector<ComputeItem*> compute_list = task->_compute_list;
        for (int i=0; i<compute_list.size(); i++) {
            ComputeItem* ci = compute_list[i];
            int dagidx = ci->_dstidx;
            pair<bool, bool> usage = ci->_usage;
            
            bool same = usage.first;
            bool diff = usage.second;

            if (same) {
                BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
                _lock.lock();
                _datamap.insert(make_pair(dagidx, queue));
                _lock.unlock();
            }

            if (diff) {
                int target = -1 * dagidx;
                if (dagidx == 0)
                    target = KEY0;

                BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
                _lock.lock();
                _datamap.insert(make_pair(target, queue));
                _lock.unlock();
            }
        }
    } else if (type == 4) {
        // persist: do not need to create queue
    }
}

void Worker::prepareDataMap(Task* task, DataMap* datamap) {

    int type = task->_type;
    if (type == 0) {
        // read
        string blk_name = task->_blk_name;
        vector<int> dagidx_list = task->_dagidx_list;
        vector<pair<bool, bool>> usage_list = task->_usage_list;

        if (blk_name == "ZERO") {
            // only generate for local blocking queue
            for (int dagidx: dagidx_list) {
                datamap->createQueue(dagidx);
            }
        } else {
            // generate based on usage
            for (int i=0; i<dagidx_list.size(); i++) {
                int dagidx = dagidx_list[i];
                pair<bool, bool> usage = usage_list[i];

                bool same = usage.first;
                bool diff = usage.second;

                if (same) {
                    datamap->createQueue(dagidx);
                }

                if (diff) {
                    int target = -1 * dagidx;
                    if (dagidx == 0)
                        target = KEY0;

                    datamap->createQueue(target);
                }
            }
        }
    } else if (type == 1) {
        // send: we have generated these queues
        //vector<int> dagidx_list;
    } else if (type == 2) {
        // recv
        vector<int> dagidx_list = task->_dagidx_list;
        for (auto dagidx: dagidx_list) {
            datamap->createQueue(dagidx);
        }
    } else if (type == 3) {
        // compute
        vector<ComputeItem*> compute_list = task->_compute_list;
        for (int i=0; i<compute_list.size(); i++) {
            ComputeItem* ci = compute_list[i];
            int dagidx = ci->_dstidx;
            pair<bool, bool> usage = ci->_usage;
            
            bool same = usage.first;
            bool diff = usage.second;

            if (same) {
                datamap->createQueue(dagidx);
            }

            if (diff) {
                int target = -1 * dagidx;
                if (dagidx == 0)
                    target = KEY0;

                datamap->createQueue(target);
            }
        }
    } else if (type == 4) {
        // persist: do not need to create queue
    }
}

void Worker::repairStripe(int batchid, int stripeid, int tasknum, vector<Task*> tasklist) {
    string prefix = "[" + to_string(batchid) + ":" + to_string(stripeid) + "] ";
    LOG << "Worker::repairStripe Start "<< prefix << ", tasknum: " << tasknum << endl;
    struct timeval time1, time2, time3, time4, time5, time6;
    gettimeofday(&time1, NULL);

    vector<Task*> readtasks;
    vector<Task*> sendtasks;
    vector<Task*> recvtasks;
    vector<Task*> computetasks;
    vector<Task*> persisttasks;

    // 0. assign task to corresponding list
    for (int i=0; i<tasknum; i++) {
        Task* curtask = tasklist[i];
        int type = curtask->_type;
        switch(type) {
            case 0: readtasks.push_back(curtask); break;
            case 1: sendtasks.push_back(curtask); break;
            case 2: recvtasks.push_back(curtask); break;
            case 3: computetasks.push_back(curtask); break;
            case 4: persisttasks.push_back(curtask); break;
        }
    }
    
    int num = 0;

    // 1. read tasks: divide read tasks into real read and virtual read
    vector<Task*> realread;
    vector<Task*> virtread;
    for (auto task: readtasks) {
        string blkname = task->_blk_name;
        if (blkname == "ZERO")
            virtread.push_back(task);
        else
            realread.push_back(task);
    }
    num += virtread.size();
    num += realread.size();
    LOG << prefix <<".realread: " << realread.size() << ", virtread: " << virtread.size() << endl;
    for (Task* read: readtasks) {
        read->dump();
    }
   
    // 2. send tasks: send task in the same level are in the same Task
    num += sendtasks.size();
    _sendNum = 0;
    for (Task* send: sendtasks) {
        send->dump();
        _sendNum += send->_dagidx_list.size();
    }
    LOG << prefix << "sendNum = " << _sendNum << endl;

    // 3. recv tasks: recv task in the same level are in the same task
    _recvNum = 0;
    num += recvtasks.size();
    for (Task* recv: recvtasks) {
        recv->dump();
        _recvNum += recv->_dagidx_list.size();
    }
    LOG << prefix << "recvNum = " << _recvNum << endl;

    // 4. compute tasks: compute task in the same level are in the same task
    num += computetasks.size();
    LOG << prefix <<".compute: " << computetasks.size()<< endl;
    for (Task* compute: computetasks) {
        compute->dump();
    }

    // 5. persist tasks
    num += persisttasks.size();
    LOG << prefix <<".persist: " << persisttasks.size()<< endl;
    for (Task* persist: persisttasks) {
        persist->dump();
    }

    LOG << prefix << "threadnum: " << num << endl;
    thread thrds[num];
    int tid=0;
    // launch threads
    // read
    for (auto item: realread) {
        thrds[tid++] = thread([=]{readRealBlock(batchid, stripeid, item);});
    }
    for (auto item: virtread) {
        thrds[tid++] = thread([=]{readVirtBlock(batchid, stripeid, item);});
    }
    // send
    for (auto item: sendtasks) {
        thrds[tid++] = thread([=]{sendOneByOne(batchid, stripeid, item);});
        // thrds[tid++] = thread([=]{sendGroups(batchid, stripeid, item);});
    }
    // recv
    for (auto item: recvtasks) {
        thrds[tid++] = thread([=]{recvOneByOne(batchid, stripeid, item);});
        //thrds[tid++] = thread([=]{recvGroups(batchid, stripeid, item);});
    }
    // compute
    for (auto item: computetasks) {
        thrds[tid++] = thread([=]{computeOneByOne(batchid, stripeid, item);});
        //thrds[tid++] = thread([=]{computeGroups(batchid, stripeid, item);});
    }
    // persist
    for (auto item: persisttasks) {
        thrds[tid++] = thread([=]{persist(batchid, stripeid, item);});
    }

    // join
    for (int i=0; i<tid; i++) {
        thrds[i].join();
    }
    gettimeofday(&time2, NULL);

    LOG << "Worker::repairStripe Done" << prefix << " duration: " << DistUtil::duration(time1, time2) << endl;

}

void Worker::readRealBlock(int batchid, int stripeid, Task* task) {
    string prefix = "[" + to_string(batchid) +  ":" + to_string(stripeid) + "]";
    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    string blkname = task->_blk_name;
    vector<int> dagidx_list = task->_dagidx_list;
    vector<pair<bool, bool>> usage_list = task->_usage_list;
    if (debug_read)
        task->dump();

    unordered_map<int, pair<bool, bool>> usage_map;
    for (int i=0; i<dagidx_list.size(); i++) {
        int dagidx = dagidx_list[i];
        pair<bool, bool> usage = usage_list[i];
        usage_map.insert(make_pair(dagidx, usage));
    }

    // 1. system information
    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 2. figure out read pattern and blocking queue
    vector<int> pattern;
    vector<int> indices;
    unordered_map<int, int> off2dagidx;
    for (int i=0; i<ecw; i++)
        pattern.push_back(0);
    
    for (int i=0; i<dagidx_list.size(); i++) {
        int dagidx = dagidx_list[i];
        bool same = usage_map[dagidx].first;
        bool diff = usage_map[dagidx].second;
        
        indices.push_back(dagidx);
        int j = dagidx % ecw;
        pattern[j] = 1;
        off2dagidx.insert(make_pair(j, dagidx));
    }
    
    sort(indices.begin(), indices.end());

    if (debug_read) {
        LOG << prefix << "        Worker::read.pattern: ";
        for (int i=0; i<ecw; i++)
            LOG << pattern[i] << " ";
        LOG << endl;
    }
    
    // 3. read data
    //string fullpath = _conf->_blkDir + "/" + blkname;
    string fullpath = DistUtil::getFullPathForBlock(_conf->_blkDir, blkname);
    LOG << "Worker::readWorker:fullpath = " << fullpath << endl;
    int fd = open(fullpath.c_str(), O_RDONLY);
    int readLen = 0, readl = 0;
    
    for (int i=0; i<pktnum; i++) {
        for (int j=0; j<ecw; j++) {
            if (pattern[j] == 0)
                continue;
            // now we read the j-th subpacket in packet i
            int dagidx = off2dagidx[j];
            bool same = usage_map[dagidx].first;
            bool diff = usage_map[dagidx].second;
            
            _lock.lock();
            BlockingQueue<DataPacket*>* readqueue = nullptr;
            BlockingQueue<DataPacket*>* cachequeue = nullptr;
            if (same) {

                /* parallel stripe */
                readqueue = _stripe2datamap[stripeid]->getQueue(dagidx);

                /* sequential stripe*/
                //readqueue = _datamap[dagidx];
            }
            if (diff) {
                /* parallel stripe */
                if (dagidx != 0)
                    cachequeue = _stripe2datamap[stripeid]->getQueue(-1*dagidx);
                else
                    cachequeue = _stripe2datamap[stripeid]->getQueue(KEY0);

                /* sequential stripe*/
                //if (dagidx != 0)
                //    cachequeue = _datamap[-1*dagidx];
                //else
                //    cachequeue = _datamap[KEY0];
            }
            _lock.unlock();
            
            int start = i * pktbytes + j * subpktbytes;
            readLen = 0;
            DataPacket* curpkt = new DataPacket(subpktbytes);
            while (readLen < subpktbytes) {
                if ((readl = pread(fd,
                                curpkt->getData() + readLen,
                                subpktbytes - readLen,
                                start)) < 0) {
                    LOG << "ERROR during disk read: " << fullpath << endl;
                } else {
                    readLen += readl;
                }
            }

            //// debug
            //if (i == 0) {
            //    cout << "READ dagidx " << dagidx << ": " << (int)curpkt->getData()[0] << endl;
            //}

            if (same && diff) {
                DataPacket* cachepkt = new DataPacket(subpktbytes);
                cachepkt->duplicateBy(curpkt);
                
                readqueue->push(curpkt);
                cachequeue->push(cachepkt);
            } else if (diff) {
                cachequeue->push(curpkt);
            } else if (same) {
                readqueue->push(curpkt);
            }
        }
    }
    close(fd);
    gettimeofday(&time2, NULL);
    LOG << "                                          readRealBlock.duration: " << DistUtil::duration(time1, time2) << endl;
    LOG << "readRealBlock " << blkname <<" from " << getTimeString(time1) << " to " << getTimeString(time2) << endl;
}

void Worker::readVirtBlock(int batchid, int stripeid, Task* task) {

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from tasklist
    if (debug_read)
        task->dump();

    vector<int> dagidx_list = task->_dagidx_list;


    // 1. system information
    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 2. generate zero bytes
    for (int i=0; i<pktnum; i++) {
        for (int j=0; j<dagidx_list.size(); j++) {
            int dagidx = dagidx_list[j];
            
            /* parallel stripe */
            _lock.lock();
            BlockingQueue<DataPacket*>* readqueue = _stripe2datamap[stripeid]->getQueue(dagidx);
            _lock.unlock();

            /* sequential stripe */
            //_lock.lock();
            //assert(_datamap.find(dagidx) != _datamap.end());
            //BlockingQueue<DataPacket*>* readqueue = _datamap[dagidx];
            //_lock.unlock();
            
            DataPacket* curpkt = new DataPacket(subpktbytes);
            readqueue->push(curpkt);
        }
    }
    gettimeofday(&time2, NULL);
    LOG << "                                          readVirtBlock.duration: " << DistUtil::duration(time1, time2) << endl;
}

void Worker::sendOneByOne(int batchid, int stripeid, Task* task) {

    if (debug_send)
        task->dump();

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<int> dagidxlist = task->_dagidx_list;
    vector<vector<int>> sendtolist = task->_send_to_list;

    string dagidx = "[";
    for(auto it : dagidxlist){
        dagidx += to_string(it) + " ";
    }
    dagidx += "]";

    vector<int> nodeidlist;
    unordered_map<int, vector<int>> dagidx2sendtolist;
    for (int i=0; i<dagidxlist.size(); i++) {
        int dagidx = dagidxlist[i];
        vector<int> list = sendtolist[i];
        dagidx2sendtolist.insert(make_pair(dagidx, list));

        for (int j=0; j<list.size(); j++) {
            int nodeid = list[j];
            if (find(nodeidlist.begin(), nodeidlist.end(), nodeid) == nodeidlist.end())
                nodeidlist.push_back(nodeid);
        }
    }

    // 1. prepare redis context
    unordered_map<int, redisContext*> redisCtxMap;
    for (int i=0; i<nodeidlist.size(); i++) {
        int nodeid = nodeidlist[i];
        unsigned int send_ip;
        if (nodeid < _conf->_agentsIPs.size())
            send_ip = _conf->_agentsIPs[nodeid];
        else {
            int idx = nodeid - _conf->_agentsIPs.size();
            send_ip = _conf->_repairIPs[idx];
        }

        redisContext* writeCtx = RedisUtil::createContext(send_ip);
        redisCtxMap.insert(make_pair(nodeid, writeCtx));
    }

    // 2. prepare blocking queue
    unordered_map<int, BlockingQueue<DataPacket*>*> sendQueueMap;
    for (int i=0; i<dagidxlist.size(); i++) {
        int dagidx = dagidxlist[i];

        /* parallel stripe */
        _lock.lock();
        BlockingQueue<DataPacket*>* queue;
        if (dagidx != 0)
            queue = _stripe2datamap[stripeid]->getQueue(-1 * dagidx);
        else
            queue = _stripe2datamap[stripeid]->getQueue(KEY0);
        _lock.unlock();

        /* sequential stripe */
        //_lock.lock();
        //BlockingQueue<DataPacket*>* queue;
        //if (dagidx != 0)
        //    queue = _datamap[-1 * dagidx];
        //else
        //    queue = _datamap[KEY0];
        //_lock.unlock();

        sendQueueMap.insert(make_pair(dagidx, queue));
    }
    gettimeofday(&time2, NULL);
    if (debug_send)
        LOG << "Worker::send.prepare.time = " << DistUtil::duration(time1, time2) << endl;

    // 3. system configuration
    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 4. send
    redisReply* rReply;
    int count = 0, replyid=0;
    BlockingQueue<int> replylist;

    for (int pktid=0; pktid<pktnum; pktid++) {
        for (auto dagidx: dagidxlist) {
            DataPacket* curslice = sendQueueMap[dagidx]->pop();
            int len = curslice->getDatalen();
            char* raw = curslice->getRaw();
            int rawlen = len + 4;

            string key = to_string(batchid)+":"+to_string(stripeid)+":dagidx:"+to_string(dagidx);

            vector<int> sendtos = dagidx2sendtolist[dagidx];
            for (auto sendto: sendtos) {
                redisContext* writeCtx = redisCtxMap[sendto];
                redisAppendCommand(writeCtx, "RPUSH %s %b", key.c_str(), raw, rawlen); count++; replylist.push(sendto);
            }

            delete curslice;

            if (count > 10) {
                int replynodeid = replylist.pop();
                redisContext* writeCtx = redisCtxMap[replynodeid];
                redisGetReply(writeCtx, (void**)&rReply); replyid++;
                if(rReply != NULL){
                    freeReplyObject(rReply);
                }else{
                    LOG << "ERROR sendOneByOne rReply == NULL" << endl;
                }
                
            }
        }
    }

    for (int i=replyid; i<count; i++) {
        int replynodeid = replylist.pop();
        redisContext* writeCtx = redisCtxMap[replynodeid];
        redisGetReply(writeCtx, (void**)&rReply); replyid++;
        freeReplyObject(rReply);
    }

    // 5. clean
    for (auto item: redisCtxMap) {
        int nodeid = item.first;
        redisContext* writeCtx = item.second;
        redisFree(writeCtx);
    }

    gettimeofday(&time2, NULL);
    LOG << "                                          sendOneByOne.duration: " << DistUtil::duration(time1, time2) << endl;
    LOG << "sendOneByOne [" << batchid << ":" << stripeid << "] for " << dagidx << "pkts from " << getTimeString(time1) << " to " << getTimeString(time2) << endl;
    //cout << " for ";
    //for (auto idx: dagidxlist) {
    //    vector<int> sendtos = dagidx2sendtolist[idx];
    //    cout << idx << " (to ";
    //    for (auto sendto: sendtos) {
    //        cout << sendto << " ";
    //    }
    //    cout << ") ";
    //}
    //cout << endl;
}

void Worker::sendGroups(int batchid, int stripeid, Task* task) {

    if (debug_send)
        task->dump();

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<int> dagidxlist = task->_dagidx_list;
    vector<vector<int>> sendtolist = task->_send_to_list;

    // 1. divide dagidx into groups
    // nodeid2dagidxlist: nodeid -> dagidxlist, only record dagidx that are sent to 1 agent
    unordered_map<int, vector<int>> nodeid2dagidxlist;
    // dagidx2nodeidlist: dagidx -> nodeidlist, record dagidx thare are sent to multiple agents
    unordered_map<int, vector<int>> dagidx2nodeidlist;

    for (int i=0; i<dagidxlist.size(); i++) {
        int dagidx = dagidxlist[i];
        vector<int> list = sendtolist[i];

        if (list.size() == 1) {
            int nodeid = list[0];
            if (nodeid2dagidxlist.find(nodeid) == nodeid2dagidxlist.end()) {
                vector<int> tmplist = {dagidx};
                nodeid2dagidxlist.insert(make_pair(nodeid, tmplist));
            } else {
                nodeid2dagidxlist[nodeid].push_back(dagidx);
            }
        } else {
            dagidx2nodeidlist.insert(make_pair(dagidx, list));
        }
    }

    // 2. issue threads
    int num = nodeid2dagidxlist.size() + dagidx2nodeidlist.size();
    thread thrds[num];
    int tid=0;
    for (auto item: nodeid2dagidxlist) {
        int nodeid = item.first;
        vector<int> list = item.second;
        thrds[tid++] = thread([=]{sendToNode(batchid, stripeid, nodeid, list);});
    }
    for (auto item:dagidx2nodeidlist) {
        int dagidx = item.first;
        vector<int> list = item.second;
        thrds[tid++] = thread([=]{sendDagidx(batchid, stripeid, dagidx, list);});
    }

    // 3. join
    for (int i=0; i<tid; i++) {
        thrds[i].join();
    }

    gettimeofday(&time2, NULL);
    LOG << "                                          sendGroups.duration: " << DistUtil::duration(time1, time2) << endl;
}

void Worker::sendToNode(int batchid, int stripeid, int nodeid, vector<int> dagidxlist) {

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. redis context for nodeid
    unsigned int send_ip;
    if (nodeid < _conf->_agentsIPs.size())
        send_ip = _conf->_agentsIPs[nodeid];
    else {
        int idx = nodeid - _conf->_agentsIPs.size();
        send_ip = _conf->_repairIPs[idx];
    }
    
    redisContext* writeCtx = RedisUtil::createContext(send_ip);

    // 1. prepare blocking queue
    unordered_map<int, BlockingQueue<DataPacket*>*> sendQueueMap;
    for (int i=0; i<dagidxlist.size(); i++) {
        int dagidx = dagidxlist[i];
        
        /* parallel stripe */
        _lock.lock();
        BlockingQueue<DataPacket*>* queue;
        if (dagidx != 0)
            queue = _stripe2datamap[stripeid]->getQueue(-1 * dagidx);
        else
            queue = _stripe2datamap[stripeid]->getQueue(KEY0);
        _lock.unlock();

        /* sequential stripe */
        //_lock.lock();
        //BlockingQueue<DataPacket*>* queue;
        //if (dagidx != 0)
        //    queue = _datamap[-1 * dagidx];
        //else
        //    queue = _datamap[KEY0];
        //_lock.unlock();

        sendQueueMap.insert(make_pair(dagidx, queue));
    }
    gettimeofday(&time2, NULL);
    if (debug_send)
        LOG << "Worker::sendToNode.prepare.time = " << DistUtil::duration(time1, time2) << endl;

    // 2. system configuration
    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 3. send
    redisReply* rReply;
    int count = 0, replyid=0;

    for (int pktid=0; pktid<pktnum; pktid++) {
        for (auto dagidx: dagidxlist) {
            DataPacket* curslice = sendQueueMap[dagidx]->pop();
            if(curslice == nullptr){
                cout << "LOG sendToNode" << endl;
            }
            int len = curslice->getDatalen();
            char* raw = curslice->getRaw();
            int rawlen = len + 4;

            string key = to_string(batchid)+":"+to_string(stripeid)+":dagidx:"+to_string(dagidx);
            redisAppendCommand(writeCtx, "RPUSH %s %b", key.c_str(), raw, rawlen); count++; 

            delete curslice;

            if (count > 10) {
                redisGetReply(writeCtx, (void**)&rReply); replyid++;
                freeReplyObject(rReply);
            }
        }
    }

    for (int i=replyid; i<count; i++) {
        redisGetReply(writeCtx, (void**)&rReply); replyid++;
        freeReplyObject(rReply);
    }

    // 5. clean
    redisFree(writeCtx);

    gettimeofday(&time2, NULL);
    LOG << "                                          sendToNode.duration: " << DistUtil::duration(time1, time2) << endl;
    //cout << " for ";
    //for (auto idx: dagidxlist) {
    //    vector<int> sendtos = dagidx2sendtolist[idx];
    //    cout << idx << " (to ";
    //    for (auto sendto: sendtos) {
    //        cout << sendto << " ";
    //    }
    //    cout << ") ";
    //}
    //cout << endl;
}

void Worker::sendDagidx(int batchid, int stripeid, int dagidx, vector<int> nodeidlist) {

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. prepare redis context
    unordered_map<int, redisContext*> redisCtxMap;
    for (int i=0; i<nodeidlist.size(); i++) {
        int nodeid = nodeidlist[i];
        unsigned int send_ip;
        if (nodeid < _conf->_agentsIPs.size())
            send_ip = _conf->_agentsIPs[nodeid];
        else {
            int idx = nodeid - _conf->_agentsIPs.size();
            send_ip = _conf->_repairIPs[idx];
        }

        redisContext* writeCtx = RedisUtil::createContext(send_ip);
        redisCtxMap.insert(make_pair(nodeid, writeCtx));
    }

    // 1. prepare blocking queue
    BlockingQueue<DataPacket*>* sendQueue;

    /* parallel stripe */
    _lock.lock();
    if (dagidx != 0)
        sendQueue = _stripe2datamap[stripeid]->getQueue(-1 * dagidx);
    else
        sendQueue = _stripe2datamap[stripeid]->getQueue(KEY0);
    _lock.unlock();
    
    /*sequential stripe */
    //_lock.lock();
    //if (dagidx != 0)
    //    sendQueue = _datamap[-1 * dagidx];
    //else
    //    sendQueue = _datamap[KEY0];
    //_lock.unlock();
    
    // 2. system configuration
    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 3. send
    redisReply* rReply;
    int count = 0, replyid=0;
    BlockingQueue<int> replylist;

    for (int pktid=0; pktid<pktnum; pktid++) {
        DataPacket* curslice = sendQueue->pop();
        int len = curslice->getDatalen();
        char* raw = curslice->getRaw();
        int rawlen = len + 4;
        
        string key = to_string(batchid)+":"+to_string(stripeid)+":dagidx:"+to_string(dagidx);
        
        for (auto sendto: nodeidlist) {
            redisContext* writeCtx = redisCtxMap[sendto];
            redisAppendCommand(writeCtx, "RPUSH %s %b", key.c_str(), raw, rawlen); count++; replylist.push(sendto);
        }
        
        delete curslice;
        
        if (count > 10) {
            int replynodeid = replylist.pop();
            redisContext* writeCtx = redisCtxMap[replynodeid];
            redisGetReply(writeCtx, (void**)&rReply); replyid++;
            freeReplyObject(rReply);
        }
    }

    for (int i=replyid; i<count; i++) {
        int replynodeid = replylist.pop();
        redisContext* writeCtx = redisCtxMap[replynodeid];
        redisGetReply(writeCtx, (void**)&rReply); replyid++;
        freeReplyObject(rReply);
    }

    // 5. clean
    for (auto item: redisCtxMap) {
        int nodeid = item.first;
        redisContext* writeCtx = item.second;
        redisFree(writeCtx);
    }

    gettimeofday(&time2, NULL);
    LOG << "                                          sendDagidx.duration: " << DistUtil::duration(time1, time2) << endl;
    //cout << " for ";
    //for (auto idx: dagidxlist) {
    //    vector<int> sendtos = dagidx2sendtolist[idx];
    //    cout << idx << " (to ";
    //    for (auto sendto: sendtos) {
    //        cout << sendto << " ";
    //    }
    //    cout << ") ";
    //}
    //cout << endl;
}

void Worker::recvOneByOne(int batchid, int stripeid, Task* task) {
    string prefix = "[" + to_string(batchid) +  ":" + to_string(stripeid) + "]";
    
    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    if (debug_recv)
        task->dump();

    // 0. get information from task
    vector<int> dagidx_list = task->_dagidx_list;
    string dagidx = "[";
    for(auto it : dagidx_list){
        dagidx += to_string(it) + " ";
    }
    dagidx += "]";
    if (debug_recv)
        LOG << "Worker::recv.dagidx_list.size = " << dagidx_list.size() << endl;

    recvList(batchid, stripeid, dagidx_list);

    gettimeofday(&time2, NULL);
    LOG << "                                             recvOneByOne.duration: " << DistUtil::duration(time1, time2) << endl;
    LOG << "recvOneByOne  " << batchid << ":" << stripeid << " for " << dagidx << "pkts from " << getTimeString(time1) << " to " << getTimeString(time2) << endl;
    //cout << " for: ";
    //for (auto dagidx: dagidx_list)
    //    cout << dagidx << " ";
    //cout << endl;
}

void Worker::recvGroups(int batchid, int stripeid, Task* task) {

    if (debug_recv)
        task->dump();

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<int> dagidx_list = task->_dagidx_list;

    if (debug_recv)
        LOG << "Worker::recv.dagidx_list.size = " << dagidx_list.size() << endl;

    int groupsize = _conf->_recvGroupSize;
    int groupnum = dagidx_list.size() / groupsize;
    if (dagidx_list.size() % groupsize > 0)
        groupnum += 1;

    // 1. divide dagidx_list into groups
    vector<vector<int>> dagidx_group;
    for (int i=0; i<groupnum; i++) {
        vector<int> list;
        dagidx_group.push_back(list);
    }
    for (int i=0; i<dagidx_list.size(); i++) {
        int dagidx = dagidx_list[i];
        int groupid = i / groupsize;
        dagidx_group[groupid].push_back(dagidx);
    }

    // 2. issue threads
    thread thrds[groupnum];
    for (int i=0; i<groupnum; i++) {
        thrds[i] = thread([=]{recvList(batchid, stripeid, dagidx_group[i]);});
    }

    // 3. join
    for (int i=0; i<groupnum; i++) {
        thrds[i].join();
    }

    gettimeofday(&time2, NULL);
    LOG << "                                             recvGroup.duration: " << DistUtil::duration(time1, time2) << endl;
    //cout << " for: ";
    //for (auto dagidx: dagidx_list)
    //    cout << dagidx << " ";
    //cout << endl;
}

void Worker::recvList(int batchid, int stripeid, vector<int> dagidx_list) {

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    if (debug_recv)
        LOG << "Worker::recvList.dagidx_list.size = " << dagidx_list.size() << endl;

    // 1. create blocking queue
    unordered_map<int, BlockingQueue<DataPacket*>*> recvQueueMap;
    for (int i=0; i<dagidx_list.size(); i++) {
        int dagidx = dagidx_list[i];
        
        /* parallel stripe */
        _lock.lock();
        recvQueueMap.insert(make_pair(dagidx, _stripe2datamap[stripeid]->getQueue(dagidx)));
        _lock.unlock();
        

        /* sequential stripe */
        //_lock.lock();
        //recvQueueMap.insert(make_pair(dagidx, _datamap[dagidx]));
        //_lock.unlock();
    }

    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    unsigned int recv_ip = _conf->_localIp;

    // 2. create redis context
    redisContext* readCtx = RedisUtil::createContext(recv_ip);
    redisReply* rReply;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 3. recv
    int count=0;
    int replyid=0;

    // recv from local context
    for (int i=0; i<pktnum; i++) {
        for (int j=0; j<dagidx_list.size(); j++) {
            int dagidx = dagidx_list[j];
            string key = to_string(batchid)+":"+to_string(stripeid)+":dagidx:"+to_string(dagidx);
            redisAppendCommand(readCtx, "blpop %s 0", key.c_str());
        }
        count++;
    }

    for (int i=replyid; i<count; i++) {
        for (int j=0; j<dagidx_list.size(); j++) {

            int dagidx = dagidx_list[j];

            redisGetReply(readCtx, (void**)&rReply);
            char* content = rReply->element[1]->str;
            DataPacket* pkt = new DataPacket(content);
                
            recvQueueMap[dagidx]->push(pkt);
            
            freeReplyObject(rReply);    
        }
    }
    
    redisFree(readCtx);

    gettimeofday(&time2, NULL);
    LOG << "                                             recvList.duration: " << DistUtil::duration(time1, time2) << endl;
    //cout << " for: ";
    //for (auto dagidx: dagidx_list)
    //    cout << dagidx << " ";
    //cout << endl;
}

void Worker::computeOneByOne(int batchid, int stripeid, Task* task) {
    string prefix = "[" + to_string(batchid) +  ":" + to_string(stripeid) + "]";
    

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<ComputeItem*> compute_list = task->_compute_list;

    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 1. prepare
    vector<int> input_list;
    unordered_map<int, int*> matrix_map;
    unordered_map<int, BlockingQueue<DataPacket*>*> writeQueueMap;
    unordered_map<int, BlockingQueue<DataPacket*>*> readQueueMap;
    for (int taskid=0; taskid<compute_list.size(); taskid++) {
        ComputeItem* ci = compute_list[taskid];
        vector<int> srclist = ci->_srclist;
        int dst = ci->_dstidx;
        vector<int> coefs = ci->_coefs;
        pair<bool, bool> usage = ci->_usage;

        // 1.0 sort input dagidx
        for (auto dagidx: srclist) {
            if (find(input_list.begin(), input_list.end(), dagidx) == input_list.end()) {
                
                input_list.push_back(dagidx);

                /* parallel stripe */
                _lock.lock();
                BlockingQueue<DataPacket*>* queue = _stripe2datamap[stripeid]->getQueue(dagidx);
                _lock.unlock();

                /* sequential stripe */
                //_lock.lock();
                //BlockingQueue<DataPacket*>* queue = _datamap[dagidx];
                //_lock.unlock();

                readQueueMap.insert(make_pair(dagidx, queue));
            }
        }

        // 1.1 generate blocking queue for outputs
        BlockingQueue<DataPacket*>* localqueue = nullptr;
        BlockingQueue<DataPacket*>* cachequeue = nullptr;
        if (usage.first) {

            /* parallel stripe */
            _lock.lock();
            writeQueueMap.insert(make_pair(dst, _stripe2datamap[stripeid]->getQueue(dst)));
            _lock.unlock();

            /* sequential stripe */
            //_lock.lock();
            //writeQueueMap.insert(make_pair(dst, _datamap[dst]));
            //_lock.unlock();
        }
        if (usage.second) {
            /* parallel stripe */
            _lock.lock();
            if (dst != 0)
                writeQueueMap.insert(make_pair(-1*dst, _stripe2datamap[stripeid]->getQueue(-1*dst)));
            else
                writeQueueMap.insert(make_pair(KEY0, _stripe2datamap[stripeid]->getQueue(KEY0)));
            _lock.unlock();

            /* sequential stripe */
            //_lock.lock();
            //if (dst != 0)
            //    writeQueueMap.insert(make_pair(-1*dst, _datamap[-1*dst]));
            //else
            //    writeQueueMap.insert(make_pair(KEY0, _datamap[KEY0]));
            //_lock.unlock();
        }

        // 1.2 prepare matrix
        int* matrix = (int*)calloc(srclist.size(), sizeof(int));
        for (int i=0; i<coefs.size(); i++) {
            matrix[i] = coefs[i];
        }
        matrix_map.insert(make_pair(dst, matrix));
    }
    gettimeofday(&time2, NULL);

    // 2. perform ec computation
    // LOG << "        Worker::compute.subpktbytes: " << subpktbytes << ", pktbytes: " << pktbytes << ", pktnum: " << pktnum << endl;

    for (int pktid=0; pktid<pktnum; pktid++) {
        // cout << "        Worker::compute pkdid: " << pktid << endl;

        // 2.0 prepare a bufmap for this iteration
        unordered_map<int, char*> bufMap;
        unordered_map<int, DataPacket*> pktMap;

        //cout << "        Worker::compute.input_list.size = " << input_list.size() << endl;
        // 2.1 for each input dagidx, get data in bufmap
        for (auto dagidx: input_list) {
            LOG << prefix << "          Worker::compute:pop dagidx: start " << dagidx << endl;
            BlockingQueue<DataPacket*>* readqueue = readQueueMap[dagidx];
            DataPacket* curpkt = readqueue->pop();
            bufMap.insert(make_pair(dagidx, curpkt->getData()));
            pktMap.insert(make_pair(dagidx, curpkt));
            LOG << prefix << "          Worker::compute:pop dagidx: end " << dagidx << endl;
        }

        // 2.2 for each task, calculate output, and push into blocking queue
        for (int taskid=0; taskid<compute_list.size(); taskid++) {
            ComputeItem* ci = compute_list[taskid];
            vector<int> srclist = ci->_srclist;
            int dst = ci->_dstidx;
            pair<bool, bool> usage = ci->_usage;
        
            bool same = usage.first;
            bool diff = usage.second;

            // prepare data packet for res
            DataPacket* respkt = new DataPacket(subpktbytes);
            bufMap.insert(make_pair(dst, respkt->getData()));
            
            int* matrix = matrix_map[dst];

            char** data = (char**)calloc(srclist.size(), sizeof(char*));
            char** code = (char**)calloc(1, sizeof(char*));
            for (int bufIdx = 0; bufIdx < srclist.size(); bufIdx++) {
                int child = srclist[bufIdx];
                assert (bufMap.find(child) != bufMap.end());
                data[bufIdx] = bufMap[child];
            }
            // prepare code buf and matrix
            code[0] = bufMap[dst];

            // perform computation 
            Computation::Multi(code, data, matrix, 1, srclist.size(), subpktbytes, "Isal"); 

            // put the respkt into blockingqueue
            if (same && diff) {
                DataPacket* cachepkt = new DataPacket(subpktbytes);
                cachepkt->duplicateBy(respkt);

                if (dst == 0)
                    writeQueueMap[KEY0]->push(cachepkt);
                else
                    writeQueueMap[-1*dst]->push(cachepkt);

                writeQueueMap[dst]->push(respkt);
            } else if (same) {
                writeQueueMap[dst]->push(respkt);
            } else if (diff) {
                if (dst == 0)
                    writeQueueMap[KEY0]->push(respkt);
                else
                    writeQueueMap[-1*dst]->push(respkt);
            }
        }

        // 2.3 clean bufMap
        for (auto item: pktMap)
            if (item.second)
                delete item.second;
        pktMap.clear();
        bufMap.clear();
    }

    // clean matrix map
    for (auto item: matrix_map) {
        if (item.second)
            free(item.second);
    }

    gettimeofday(&time3, NULL);
    LOG << "                   computeOneByOne.duration: " << DistUtil::duration(time1, time2)  << endl;
    if (debug_compute)
        task->dump();
    LOG << "compute for " << batchid << ":" << stripeid << " from " << getTimeString(time1) << " to " << getTimeString(time2) << endl
         << "       prepare data " << DistUtil::duration(time1, time2)  << endl
         << "       perform comput " << DistUtil::duration(time1, time2)  << endl;
}

void Worker::computeGroups(int batchid, int stripeid, Task* task) {

    if (debug_compute)
        task->dump();

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<ComputeItem*> compute_list = task->_compute_list;

    // 1. divide computeitems into compute groups
    vector<ComputeGroup*> compute_groups;
    for (int i=0; i<compute_list.size(); i++) {
        ComputeItem* ci = compute_list[i];

        int groupid = -1;
        for (int j=0; j<compute_groups.size(); j++) {
            ComputeGroup* cg = compute_groups[j];
            if (cg->match(ci)) {
                // ci can be added into the current compute group
                groupid = j;
                cg->add(ci);
                break;
            }
        }

        // we do not find a compute group that can include the current compute item
        // just create a new group
        if (groupid == -1) {
            ComputeGroup* cg = new ComputeGroup();
            cg->add(ci);
            compute_groups.push_back(cg);
        }
    }

    // 2. merge compute groups
    vector<ComputeGroup*> merge_compute_groups;
    int computeSize = _conf->_computeGroupSize;
    for (int i=0; i<compute_groups.size(); i++) {
        ComputeGroup* curcg = compute_groups[i];

        int mergeid = -1;
        for (int j=0; j<merge_compute_groups.size(); j++) {
            ComputeGroup* tomergecg = merge_compute_groups[j];
            if (tomergecg->canMerge(curcg, computeSize)) {
                mergeid = j;
                tomergecg->merge(curcg);
                delete curcg;
                break;
            }
        }

        // we do not find a compute group that can merge the current compute group
        // just add it as a new merged compute group
        if (mergeid == -1) {
            merge_compute_groups.push_back(curcg);
        }
    }

    // 3. for each merged compute group issue a thread
    int groupnum = merge_compute_groups.size();
    LOG << "Worker::mergecomputeGroups.size = " << groupnum << endl;
    thread thrds[groupnum];
    for (int i=0; i<groupnum; i++) {
        thrds[i] = thread([=]{computeList(batchid, stripeid, merge_compute_groups[i]);});
    }

    // 3. join
    for (int i=0; i<groupnum; i++) {
        thrds[i].join();
    }

    gettimeofday(&time2, NULL);
    LOG << "                                             computeGroups.duration: " << DistUtil::duration(time1, time2)  << endl;
}

void Worker::computeList(int batchid, int stripeid, ComputeGroup* cg) {

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<ComputeItem*> compute_list = cg->_compute_list;
    unordered_map<int, int> srcmap = cg->_src_map;

    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    // 1. prepare
    vector<int> input_list;
    vector<int> output_list;
    unordered_map<int, BlockingQueue<DataPacket*>*> writeQueueMap;
    unordered_map<int, BlockingQueue<DataPacket*>*> readQueueMap;
    unordered_map<int, pair<bool, bool>> usageMap;

    // 1.0 sort input dagidx
    for (auto item: srcmap) {
        int dagidx = item.first;
        input_list.push_back(dagidx);

        /* parallel stripe */
        _lock.lock();
        BlockingQueue<DataPacket*>* queue = _stripe2datamap[stripeid]->getQueue(dagidx);
        _lock.unlock();
        
        /* sequential stripe */
        //_lock.lock();
        //BlockingQueue<DataPacket*>* queue = _datamap[dagidx];
        //_lock.unlock();
        
        readQueueMap.insert(make_pair(dagidx, queue));
    }

    // 1.1 prepare encoding matrix
    int row = compute_list.size();
    int col = input_list.size();
    int* matrix = (int*)calloc(row * col, sizeof (int));
    char c = 0;
    memset(matrix, c, row*col*sizeof(int));
    
    for (int taskid=0; taskid<compute_list.size(); taskid++) {
        ComputeItem* ci = compute_list[taskid];
        vector<int> srclist = ci->_srclist;
        int dst = ci->_dstidx;
        vector<int> coefs = ci->_coefs;
        pair<bool, bool> usage = ci->_usage;

        output_list.push_back(dst);
        usageMap.insert(make_pair(dst, usage));

        // 1.2 set corresponding coef in matrix
        unordered_map<int, int> dagidx2coef;
        for (int srcid=0; srcid<srclist.size(); srcid++) {
            int dagidx = srclist[srcid];
            int coef = coefs[srcid];
            dagidx2coef.insert(make_pair(dagidx, coef));
        }

        for (int inputid=0; inputid<input_list.size(); inputid++) {
            int dagidx = input_list[inputid];
            if (dagidx2coef.find(dagidx) == dagidx2coef.end())
                continue;

            int coef = dagidx2coef[dagidx];
            matrix[taskid*col+inputid] = coef;
        }

        // 1.3 generate blocking queue for outputs
        BlockingQueue<DataPacket*>* localqueue = nullptr;
        BlockingQueue<DataPacket*>* cachequeue = nullptr;
        if (usage.first) {

            /* parallel stripe */
            _lock.lock();
            writeQueueMap.insert(make_pair(dst, _stripe2datamap[stripeid]->getQueue(dst)));
            _lock.unlock();

            /* sequential stripe */
            //_lock.lock();
            //writeQueueMap.insert(make_pair(dst, _datamap[dst]));
            //_lock.unlock();
        }
        if (usage.second) {
        
            /* parallel stripe */
            _lock.lock();
            if (dst != 0)
                writeQueueMap.insert(make_pair(-1*dst, _stripe2datamap[stripeid]->getQueue(-1*dst)));
            else
                writeQueueMap.insert(make_pair(KEY0, _stripe2datamap[stripeid]->getQueue(KEY0)));
            _lock.unlock();

            /* sequential stripe */
            //_lock.lock();
            //if (dst != 0)
            //    writeQueueMap.insert(make_pair(-1*dst, _datamap[-1*dst]));
            //else
            //    writeQueueMap.insert(make_pair(KEY0, _datamap[KEY0]));
            //_lock.unlock();
        }
    }

    // 2. perform ec computation
    // LOG << "        Worker::compute.subpktbytes: " << subpktbytes << ", pktbytes: " << pktbytes << ", pktnum: " << pktnum << endl;

    char** data = (char**)calloc(col, sizeof(char*));
    char** code = (char**)calloc(row, sizeof(char*));
    unordered_map<int, DataPacket*> pktMap;
    vector<DataPacket*> toremove;

    for (int pktid=0; pktid<pktnum; pktid++) {
        //cout << "        Worker::compute pkdid: " << pktid << endl;
        
        // 2.0 for each input dagidx, get data
        for (int inputid=0; inputid<input_list.size(); inputid++) {
            int dagidx = input_list[inputid];
            BlockingQueue<DataPacket*>* readqueue = readQueueMap[dagidx];
            DataPacket* curpkt = readqueue->pop();
            toremove.push_back(curpkt);

            data[inputid] = curpkt->getData();
        }

        // 2.2 for each output dagidx, create space
        for (int outputid=0; outputid<output_list.size(); outputid++) {
            int dagidx = output_list[outputid];
            DataPacket* respkt = new DataPacket(subpktbytes);
            pktMap.insert(make_pair(dagidx, respkt));

            code[outputid] = respkt->getData();
        }

        // 2.3 compute
        Computation::Multi(code, data, matrix, row, col, subpktbytes, "Isal");

        // 2.4 for each task, push res into proper blocking queue
        for (auto item: usageMap) {
            int dst = item.first;
            pair<bool, bool> usage = item.second;
        
            bool same = usage.first;
            bool diff = usage.second;

            DataPacket* respkt = pktMap[dst];

            // put the respkt into blockingqueue
            if (same && diff) {
                DataPacket* cachepkt = new DataPacket(subpktbytes);
                cachepkt->duplicateBy(respkt);

                if (dst == 0)
                    writeQueueMap[KEY0]->push(cachepkt);
                else
                    writeQueueMap[-1*dst]->push(cachepkt);

                writeQueueMap[dst]->push(respkt);
            } else if (same) {
                writeQueueMap[dst]->push(respkt);
            } else if (diff) {
                if (dst == 0)
                    writeQueueMap[KEY0]->push(respkt);
                else
                    writeQueueMap[-1*dst]->push(respkt);
            }
        }

        // 2.3 clean toremove
        for (DataPacket* pkt: toremove)
            if (pkt)
                delete pkt;
        pktMap.clear();
        toremove.clear();
    }

    free(matrix);
    free(data);
    free(code);

    gettimeofday(&time2, NULL);
    LOG << "                                             computeList.duration: " << DistUtil::duration(time1, time2)  << endl;
}

void Worker::persist(int batchid, int stripeid, Task* task) {
    string prefix = "[" + to_string(batchid) +  ":" + to_string(stripeid) + "]";
    if (debug_persist)
        task->dump();

    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    // 0. get information from task
    vector<int> collect_list = task->_collect_list;
    string repaired_blkname = task->_repaired_block;
    string fullpath = _conf->_blkDir + "/" + repaired_blkname + ".repair";

    int blkbytes = _conf->_blkBytes;
    int pktbytes = _conf->_pktBytes;
    int ecw = _conf->_ecw;
    pktbytes = pktbytes * ecw;

    int subpktbytes = pktbytes / ecw;
    int pktnum = blkbytes / pktbytes;

    ofstream ofs(fullpath);
    ofs.close();
    ofs.open(fullpath, ios::app);

    // 1. concatenate
    for (int i=0; i<pktnum; i++) {
        for (int j=0; j<collect_list.size(); j++) {
            int dagidx = collect_list[j];

            /* parallel stripe */
            _lock.lock();
            BlockingQueue<DataPacket*>* readqueue = _stripe2datamap[stripeid]->getQueue(dagidx);
            _lock.unlock();

            /* sequential stripe */
            //_lock.lock();
            //BlockingQueue<DataPacket*>* readqueue = _datamap[dagidx];
            //_lock.unlock();

            DataPacket* curPkt = readqueue->pop();
            int len = curPkt->getDatalen();
            if (len) {
                ofs.write(curPkt->getData(), len);
            } else
                break;
            delete curPkt;
        }
    }

    ofs.close();

    gettimeofday(&time2, NULL);
    LOG << "                                             persist.duration: " << DistUtil::duration(time1, time2)  << endl;
    LOG << "persist for " << batchid << ":" << stripeid << " from " << getTimeString(time1) << " to " << getTimeString(time2) << endl;
}

void Worker::sendFinishFlag(int batchid) {

    // 0. create context
    unsigned int coorip = _conf->_coorIp;
    redisContext* writeCtx = RedisUtil::createContext(coorip);
    redisReply* rReply;

    // 1. figure out the current nodeid
    unsigned int localip = _conf->_localIp;
    int nodeid = -1;
    for (int i=0; i<_conf->_agentsIPs.size(); i++) {
        LOG << "check localip " << RedisUtil::ip2Str(localip) << " with agentip " << RedisUtil::ip2Str(_conf->_agentsIPs[i]) << endl;
        if (localip == _conf->_agentsIPs[i]) {
            nodeid = i;
            break;
        }
    }

    if (nodeid == -1) {
        for (int i=0; i<_conf->_repairIPs.size(); i++) {
            LOG << "check localip " << RedisUtil::ip2Str(localip) << " with repairip " << RedisUtil::ip2Str(_conf->_repairIPs[i]) << endl;
            if (localip == _conf->_repairIPs[i]) {
                nodeid = _conf->_agentsIPs.size() + i;
                break;
            }
        }
    }

    if (nodeid == -1) {
        LOG << "ERROR! nodeid == -1" << endl;
    }

    string key = to_string(batchid)+":"+to_string(nodeid)+":finish";
    int value = 1;
    redisAppendCommand(writeCtx, "RPUSH %s %b", key.c_str(), &value, sizeof(int));
    redisGetReply(writeCtx, (void**)&rReply);
    freeReplyObject(rReply);
    redisFree(writeCtx);

    LOG << "-------- nodeid " << nodeid << " send finish flag to coordinator --------" << endl;
}

double Worker::getTimeStamp(struct timeval tv) {
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

string Worker:: getTimeString(struct timeval tv) {
    time_t sec = tv.tv_sec;
    struct tm* timeinfo = localtime(&sec);

    char buffer[80];
    strftime(buffer, 80, "%M:%S", timeinfo);

    std::string result(buffer);
    result += "." + std::to_string(tv.tv_usec);

    return result;
}
