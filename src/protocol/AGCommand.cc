#include "AGCommand.hh"

AGCommand::AGCommand() {
  _agCmd = (char*)calloc(AGENT_COMMAND_LEN, sizeof(char));
  _cmLen = 0;
  _rKey = "ag_request";
  _maxLen = AGENT_COMMAND_LEN;
}

AGCommand::~AGCommand() {
  if (_agCmd) {
    free(_agCmd);
    _agCmd = 0;
  }
  _cmLen = 0;
}

AGCommand::AGCommand(char* reqStr) {
  _agCmd = reqStr;
  _cmLen = 0; 
  _maxLen = AGENT_COMMAND_LEN;

  // 0. read batch id
  _batch_id = readInt();

  // 1. read num stripes
  _num_stripes = readInt();

  for (int i=0; i<_num_stripes; i++) {
      int stripeid = readInt();
      int numtasks = readInt();

      _stripe_id_list.push_back(stripeid);
      _stripe_task_num.push_back(numtasks);
  }

  _agCmd = nullptr;
  _cmLen = 0;
}

void AGCommand::writeInt(int value) {
    if (_cmLen + 4 > _maxLen) {
        // the current _agCmd cannot serve the request
        cout << "AGCommand::writeInt.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
        _maxLen = _maxLen * 2;
        char* newcmd = (char*)calloc(_maxLen, sizeof(char));
        memcpy(newcmd, _agCmd, _cmLen);
        free(_agCmd);
        _agCmd = newcmd;
    }
    int tmpv = htonl(value);
    memcpy(_agCmd + _cmLen, (char*)&tmpv, 4); _cmLen += 4;
}

void AGCommand::writeString(string s) {
    int slen = s.length();
    int tmpslen = htonl(slen);
    
    if (_cmLen + 4 + slen > _maxLen) {
        // the current _agCmd cannot serve the request
        cout << "AGCommand::writeString.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
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

int AGCommand::readInt() {

    //if (_cmLen + 4 > _maxLen) {
    //    // the current _agCmd cannot serve the request
    //    cout << "AGCommand::readInt.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
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

string AGCommand::readString() {
    string toret;
    int slen = readInt();

    //if (_cmLen + slen > _maxLen) {
    //    // the current _agCmd cannot serve the request
    //    cout << "AGCommand::readString.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
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

void AGCommand::buildAGCommand(int batchid, int nstripes, 
        vector<int> stripelist, vector<int> numlist) {

    // 0. batchid
    writeInt(batchid);

    // 1. nstripes
    writeInt(nstripes);

    // 2. stripe|num
    for (int i=0; i<nstripes; i++) {
        writeInt(stripelist[i]);
        writeInt(numlist[i]);
    }
}

void AGCommand::checkLength() {
    if (_cmLen > AGENT_COMMAND_LEN) {
        cout << "ERROR::AGCommand length " << _cmLen << " is larger than AGENT_COMMAND_LEN of " << AGENT_COMMAND_LEN << endl;
    }
}

int AGCommand::getBatchId() {
    return _batch_id;
}

int AGCommand::getNumStripes() {
    return _num_stripes;
}

vector<int> AGCommand::getStripeIdList() {
    return _stripe_id_list;
}

vector<int> AGCommand::getStripeTaskNum() {
    return _stripe_task_num;
}

void AGCommand::sendTo(unsigned int ip) {
    redisContext* sendCtx = RedisUtil::createContext(ip);
    redisReply* sendReply = (redisReply*)redisCommand(sendCtx, "RPUSH ag_request %b", _agCmd, _cmLen);
    freeReplyObject(sendReply);
    redisFree(sendCtx);

    //cout << "AGCommand::sendTo.command len = " << _cmLen << endl;
    //if (_cmLen > AGENT_COMMAND_LEN)
    //    cout << "ERROR::AGCommand exceeds max command length "  << AGENT_COMMAND_LEN << endl;
}

