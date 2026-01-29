#include "NodeBatchTask.hh"

NodeBatchTask::NodeBatchTask(int batchid, vector<int> stripeidlist, vector<int> numlist, 
        unordered_map<int, vector<Task*>> taskmap, unsigned int ip) {

    _batch_id = batchid;
    _stripe_id_list = stripeidlist;
    _num_list = numlist;
    _taskmap = taskmap;
    _ip = ip;
}

NodeBatchTask::~NodeBatchTask() {

    for (auto item: _taskmap) {
        vector<Task*> list = item.second;
        for (Task* t: list) {
            if (t)
                delete t;
        }
    }
}

void NodeBatchTask::sendAGCommand(unsigned int ip) {
    AGCommand* agCmd = new AGCommand();
    agCmd->buildAGCommand(_batch_id, _stripe_id_list.size(), _stripe_id_list, _num_list);
    agCmd->sendTo(ip);

    delete agCmd;
}

void NodeBatchTask::sendTaskCommands(unsigned int ip) {

    vector<string> sendkeys;
    vector<char*> sendcmds;
    vector<int> sendlens;

    for (int i=0; i<_stripe_id_list.size(); i++) {
        int stripeid = _stripe_id_list[i];
        int tasknum = _num_list[i];

        vector<Task*> tasklist = _taskmap[stripeid];
        for (int taskid=0; taskid<tasknum; taskid++) {
            Task* curtask = tasklist[taskid];
            curtask->buildTaskCommand();

            string key = curtask->_rKey; sendkeys.push_back(key);
            char* cmd = curtask->_agCmd; sendcmds.push_back(cmd);
            int cmdlen = curtask->_cmLen; sendlens.push_back(cmdlen);

            if (curtask->_type == 3) {
                for (ComputeItem* ci: curtask->_compute_list) {
                    string ckey = ci->_cKey; sendkeys.push_back(ckey);
                    char* ccmd = ci->_cmd; sendcmds.push_back(ccmd);
                    int clen = ci->_cmdLen; sendlens.push_back(clen);
                }
            }
        }
    }

    int count=0;
    int replyid=0;

    redisContext* sendCtx = RedisUtil::createContext(ip);
    redisReply* rReply;
    for (int i=0; i<sendkeys.size(); i++) {
        string key = sendkeys[i];
        char* cmd = sendcmds[i];
        int len = sendlens[i];
        redisAppendCommand(sendCtx, "RPUSH %s %b", key.c_str(), cmd, len); count++;

        if (count > 10) {
            redisGetReply(sendCtx, (void**)&rReply); replyid++; 
            freeReplyObject(rReply);
        }
    }

    for (int i=replyid; i<count; i++) {
        redisGetReply(sendCtx, (void**)&rReply); replyid++;
        freeReplyObject(rReply);
    }
}

void NodeBatchTask::sendCommands() {
    sendAGCommand(_ip);
    sendTaskCommands(_ip);
}

void NodeBatchTask::waitFinishFlag(int nodeid, unsigned int coorip) {
    struct timeval time1; 
    
    cout<<"NodeBatchTask::waitFinishFlag start"<<endl;
    redisContext* recvCtx = RedisUtil::createContext(coorip);
    //cout<<coorip<<endl;

    string rkey = to_string(_batch_id)+":"+to_string(nodeid)+":finish";
    cout << "-------- wait " << rkey << " --------" << endl;
    redisReply* rReply;
    rReply = (redisReply*)redisCommand(recvCtx, "blpop %s 0", rkey.c_str());
    char* reqStr = rReply -> element[1] -> str;
    freeReplyObject(rReply);

    redisFree(recvCtx);
    gettimeofday(&time1, NULL);
    cout << "-------- get finish flag from node " << nodeid << " at " << getTimeString(time1) << endl;
}

string NodeBatchTask::getTimeString(struct timeval tv) {
    time_t sec = tv.tv_sec;
    struct tm* timeinfo = localtime(&sec);

    char buffer[80];
    strftime(buffer, 80, "%H:%M:%S", timeinfo);

    std::string result(buffer);
    result += "." + std::to_string(tv.tv_usec);

    return result;
}

