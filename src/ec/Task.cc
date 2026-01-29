#include "Task.hh"

Task::Task(int batchid, int stripeid) {
    _batch_id = batchid;
    _stripe_id = stripeid;
    _maxLen = TASK_COMMAND_LEN;
}

Task::Task(char* reqStr, int batchid, int stripeid, unsigned int ip) {
    _agCmd = reqStr;
    _cmLen = 0;
    _batch_id = batchid;
    _stripe_id = stripeid;

    // parse type
    _type = readInt();

    switch(_type) {
        case 0: resolveReadTask(); break;
        case 1: resolveSendTask(); break;
        case 2: resolveRecvTask(); break;
        case 3: resolveComputeTask(ip); break;
        case 4: resolvePersistTask(); break;
        default: break;
    }
    _agCmd = nullptr;
    _cmLen = 0;
    _maxLen = TASK_COMMAND_LEN;
}

Task::~Task() {
    if (_agCmd)
        free(_agCmd);

    if (_type == 3) {
        for (int i=0; i<_compute_list.size(); i++)
            delete _compute_list[i];
    }
}

void Task::buildReadTask(int type, string blockname, 
        vector<int> dagidxlist, vector<pair<bool, bool>> usagelist) {
    _type = type;
    _blk_name = blockname;
    _dagidx_list = dagidxlist;
    _usage_list = usagelist;
}

void Task::buildSendTask(int type, vector<int> dagidxlist, vector<vector<int>> sendtolist) {
    _type = type;
    _dagidx_list = dagidxlist;
    _send_to_list = sendtolist;

    //cout << "Task::buildSendTask.dagidxlist.size = " << dagidxlist.size() << endl;
    //int total = 0;
    //for (int i=0; i<dagidxlist.size(); i++) {
    //    vector<int> list = sendtolist[i];
    //    cout << "    send to " << list.size() << " agents" << endl;
    //    total += list.size();
    //}
    //cout << "    Total: " << total << endl;
}

void Task::buildRecvTask(int type, vector<int> dagidxlist) {
    _type = type;
    _dagidx_list = dagidxlist;
    //cout << "Task::buildRecvTask.dagidx_list.size = " << dagidxlist.size() << endl;
}

void Task::buildComputeTask(int type, vector<ComputeItem*> clist) {
    _type = type;
    _compute_list = clist;
}

void Task::buildPersistTask(int type, vector<int> clist, string blkname) {
    _type = type;
    _collect_list = clist;
    _repaired_block = blkname;
}

void Task::buildTaskCommand() {
    _agCmd = (char*)calloc(TASK_COMMAND_LEN, sizeof(char));
    _cmLen = 0;
    _rKey = to_string(_batch_id) + ":" + to_string(_stripe_id) + ":command";

    if (_type == 0) {
        buildReadTaskCommand();
    } else if (_type == 1) {
        buildSendTaskCommand();
    } else if (_type == 2) {
        buildRecvTaskCommand();
    } else if (_type == 3) {
        buildComputeTaskCommand();
    } else if (_type == 4) {
        buildPersistTaskCommand();
    }
}

void Task::buildReadTaskCommand() {

    // 0. type
    writeInt(_type);
    // 1. blkname
    writeString(_blk_name);
    // 2. dagidxlist
    writeInt(_dagidx_list.size());
    for (int i=0; i<_dagidx_list.size(); i++)
        writeInt(_dagidx_list[i]);
    // 3. usagelist
    if (_blk_name != "ZERO") {
        for (int i=0; i<_usage_list.size(); i++) {
            pair<bool, bool> usage = _usage_list[i];
            if (usage.first)
                writeInt(1);
            else
                writeInt(0);
            
            if (usage.second)
                writeInt(1);
            else 
                writeInt(0);
        }
    }
}

void Task::resolveReadTask() {

    // 1. blkname
    _blk_name = readString();
    // 2. dagidxlist
    int num = readInt();
    for (int i=0; i<num; i++) {
        int dagidx = readInt();
        _dagidx_list.push_back(dagidx);
    }
    // 3. usage
    if (_blk_name != "ZERO") {
        for (int i=0; i<num; i++) {
            bool same, diff;
            int sameint = readInt();
            if (sameint == 1)
                same = true;
            else
                same = false;
            
            int diffint = readInt();
            if (diffint == 1)
                diff = true;
            else
                diff = false;
            
            pair<bool, bool> usage = make_pair(same, diff);
            _usage_list.push_back(usage);
        }
    }
}

void Task::buildSendTaskCommand() {
    // 0. type
    writeInt(_type);
    // 1. dagidx list
    writeInt(_dagidx_list.size());
    for (int i=0; i<_dagidx_list.size(); i++)
        writeInt(_dagidx_list[i]);
    // 2. send to list
    for (int i=0; i<_dagidx_list.size(); i++) {
        vector<int> sendtos = _send_to_list[i];
        writeInt(sendtos.size());
        for (int j=0; j<sendtos.size(); j++)
            writeInt(sendtos[j]);
    }
}

void Task::resolveSendTask() {
    // 1. dagidx list
    int num = readInt();
    for (int i=0; i<num; i++) {
        int dagidx = readInt();
        _dagidx_list.push_back(dagidx);
    }
    // 2. send to list
    for (int i=0; i<num; i++) {
        int nodenum = readInt();
        vector<int> list;
        for (int j=0; j<nodenum; j++) {
            int nodeid = readInt();
            list.push_back(nodeid);
        }
        _send_to_list.push_back(list);
    }
}

void Task::buildRecvTaskCommand() {
    // 0. type
    writeInt(_type);
    // 1. dagidx list
    writeInt(_dagidx_list.size());
    for (int i=0; i<_dagidx_list.size(); i++)
        writeInt(_dagidx_list[i]);
}

void Task::resolveRecvTask() {
    // 1. dagidx list
    int num = readInt();
    for (int i=0; i<num; i++) {
        int dagidx = readInt();
        _dagidx_list.push_back(dagidx);
    }
}

void Task::buildComputeTaskCommand() {
    // 0. type
    writeInt(_type);
    // 1. number of compute items
    writeInt(_compute_list.size());

    // 2. target dagidx to compute
    for (int i=0; i<_compute_list.size(); i++) {
        ComputeItem* ci = _compute_list[i];
        int dstidx = ci->_dstidx;
        writeInt(dstidx);
    }

    // 3. for each ci, we generate an independent compute command
    for (int i=0; i<_compute_list.size(); i++) {
        ComputeItem* ci = _compute_list[i];

        string cKey = _rKey + ":compute:"+to_string(ci->_dstidx);
        ci->buildCommand(cKey);
    }
}

void Task::resolveComputeTask(unsigned int ip) {

    redisContext* recvCtx = RedisUtil::createContext(ip);

    // 1. number of compute items
    int num = readInt();
    // 2. target dagidx to compute
    for (int i=0; i<num; i++) {
        int dagidx = readInt();

    // 3. resolve compute item one by one
        string cKey = to_string(_batch_id)+":"+to_string(_stripe_id)+":command:compute:"+to_string(dagidx);

        redisReply* rReply;
        rReply = (redisReply*)redisCommand(recvCtx, "blpop %s 0", cKey.c_str());
        char* reqStr = rReply -> element[1] -> str;
        ComputeItem* ci = new ComputeItem(reqStr);
        _compute_list.push_back(ci);

        freeReplyObject(rReply);
    }

    redisFree(recvCtx);
}

void Task::buildPersistTaskCommand() {
    // 0. type
    writeInt(_type);
    // 1. num srcs
    writeInt(_collect_list.size());
    // 2. srclist
    for (int i=0; i<_collect_list.size(); i++)
        writeInt(_collect_list[i]);
    // 3. repaired block name
    writeString(_repaired_block);
}

void Task::resolvePersistTask() {
    // 1. num srcs
    int num = readInt();
    // 2. srclist
    for (int i=0; i<num; i++) {
        int dagidx = readInt();
        _collect_list.push_back(dagidx);
    }
    // 3. reapired block name
    _repaired_block = readString();
}

void Task::sendTo(unsigned int ip) {
    redisContext* sendCtx = RedisUtil::createContext(ip);
    redisReply* sendReply = (redisReply*)redisCommand(sendCtx, "RPUSH %s %b", _rKey.c_str(), _agCmd, _cmLen);
    freeReplyObject(sendReply);
    redisFree(sendCtx);

    //cout << "Task::send command len = " << _cmLen << endl;
    //if (_cmLen > TASK_COMMAND_LEN)
    //    cout << "ERROR::Task: cmLen = " << _cmLen << endl;

    // if it is compute task
    if (_type == 3) {
        for (int i=0; i<_compute_list.size(); i++) {
            ComputeItem* ci = _compute_list[i];
            ci->sendTo(ip);
        }
    }
}

void Task::writeInt(int value) {
    if (_cmLen + 4 > _maxLen) {
        cout << "Task::writeInt.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
        _maxLen = _maxLen * 2;
        char* newcmd = (char*)calloc(_maxLen, sizeof(char));
        memcpy(newcmd, _agCmd, _cmLen);
        free(_agCmd);
        _agCmd = newcmd;
    }
    int tmpv = htonl(value);
    memcpy(_agCmd + _cmLen, (char*)&tmpv, 4); _cmLen += 4;
}

void Task::writeString(string s) {
    int slen = s.length();
    int tmpslen = htonl(slen); 

    if (_cmLen + 4 + slen > _maxLen) {
        cout << "Task::writeString.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
        _maxLen = _maxLen * 2;
        char* newcmd = (char*)calloc(_maxLen, sizeof(char));
        memcpy(newcmd, _agCmd, _cmLen);
        free(_agCmd);
        _agCmd = newcmd;
    }
    // string length
    memcpy(_agCmd + _cmLen, (char*)&tmpslen, 4); _cmLen += 4;
    // string
    memcpy(_agCmd + _cmLen, s.c_str(), slen); _cmLen += slen;
}

int Task::readInt() {
    //if (_cmLen + 4 > _maxLen) {
    //    cout << "Task::readInt.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
    //    _maxLen = _maxLen * 2;
    //    char* newcmd = (char*)calloc(_maxLen, sizeof(char));
    //    memcpy(newcmd, _agCmd, _cmLen);
    //    free(_agCmd);
    //    _agCmd = newcmd;
    //}
    int tmpint;
    memcpy((char*)&tmpint, _agCmd + _cmLen, 4); _cmLen += 4;
    return ntohl(tmpint);
}

string Task::readString() {
    string toret;
    int slen = readInt();

    //if (_cmLen + slen > _maxLen) { 
    //    cout << "Task::readString.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
    //    _maxLen = _maxLen * 2;
    //    char* newcmd = (char*)calloc(_maxLen, sizeof(char));
    //    memcpy(newcmd, _agCmd, _cmLen); 
    //    free(_agCmd);
    //    _agCmd = newcmd;
    //}

    char* sname = (char*)calloc(sizeof(char), slen+1);
    memcpy(sname, _agCmd + _cmLen, slen); _cmLen += slen;
    toret = string(sname);
    free(sname);
    return toret;
}

void Task::dump() {
    string dumpStr;
    dumpStr += "Task::dump" + to_string(_batch_id) + ":" + to_string(_stripe_id) + "  ";
    if (_type == 0) {
        dumpStr += "  ReadTask: blockname: " + _blk_name +", dagidxlist: ";
        for (int i=0; i<_dagidx_list.size(); i++) {
            int dagidx = _dagidx_list[i];
            if (_blk_name != "ZERO") {
                pair<bool, bool> usage = _usage_list[i];
                dumpStr +=  to_string(dagidx) + " (" + to_string(usage.first) +", " + to_string(usage.second) + ") ";    
            } else {
                dumpStr +=  to_string(dagidx) + " ";
            }
        }
        LOG << dumpStr << endl;
    } else if (_type == 1) {
        dumpStr += "  SendTask: ";
        for (int i=0; i<_dagidx_list.size(); i++) {
            int dagidx = _dagidx_list[i];
            vector<int> sendtos = _send_to_list[i];
            dumpStr += "            dagidx: " + to_string(dagidx) + ", sendto: ";
            for (int j=0; j<sendtos.size(); j++)
                dumpStr += to_string(sendtos[j]) + " ";
            dumpStr += "\n";
        }
        LOG << dumpStr << endl;
    } else if (_type == 2) {
        dumpStr +=  "  RecvTask: dagidx: ";
        for (int i=0; i<_dagidx_list.size(); i++)
            dumpStr += to_string(_dagidx_list[i]) + " ";
        LOG << dumpStr << endl;
    } else if (_type == 3) {
        dumpStr += "  ComputeTask: \n"; 
        for (int i=0; i<_compute_list.size(); i++) {
            ComputeItem* ci = _compute_list[i];

            int dstidx = ci->_dstidx;
            vector<int> srclist = ci->_srclist;
            vector<int> coefs = ci->_coefs;
            dumpStr += "            Compute: dstidx " + to_string(dstidx) + "; srclist: ";
            for (int ii=0; ii<srclist.size(); ii++)
                dumpStr +=  to_string(srclist[ii]) + " ";
            dumpStr += "; coefs: ";
            for (int ii=0; ii<coefs.size(); ii++)
                dumpStr += to_string(coefs[ii]) + " ";
            dumpStr += "; usage: (" + to_string(ci->_usage.first) + ", " + to_string(ci->_usage.second) + ") \n"  ;
        }
        LOG << dumpStr << endl;
    } else if (_type == 4) {
        dumpStr += "  PersistTask: " + _repaired_block + ", collect: ";
        for (int i=0; i<_collect_list.size(); i++) {
            dumpStr += to_string(_collect_list[i]) + " ";
        }
        dumpStr += "\n";
        LOG << dumpStr << endl;
    }
}
