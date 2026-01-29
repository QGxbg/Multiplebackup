#ifndef _TASK_HH_
#define _TASK_HH_

#include "../inc/include.hh"
#include "../util/RedisUtil.hh"

using namespace std;

class ComputeItem {
    public:
        int _dstidx;
        vector<int> _srclist;
        vector<int> _coefs;
        pair<bool, bool> _usage;

        char* _cmd;
        int _cmdLen;
        string _cKey;
        int _maxLen;

        ComputeItem(int dstidx, vector<int> srclist, vector<int> coefs, pair<bool, bool> usage) {
            _dstidx = dstidx;
            _srclist = srclist;
            _coefs = coefs;
            _usage = usage;

            _cmd = NULL;
            _cmdLen = 0;
            _maxLen = COMPUTE_COMMAND_LEN;
        }

        ComputeItem(char* reqStr) {
            _cmd = reqStr;
            _cmdLen = 0;
            _maxLen = COMPUTE_COMMAND_LEN;

            // 0. dstidx
            _dstidx = readInt();
            // 1. num srcs
            int numsrc = readInt();
            // 2. srclist
            for (int i=0; i<numsrc; i++) {
                int dagidx = readInt();
                _srclist.push_back(dagidx);
            }
            // 3. coefs
            for (int i=0; i<numsrc; i++) {
                int c = readInt();
                _coefs.push_back(c);
            }
            // 4. same
            int res = readInt();
            bool same = false;
            bool diff = false;
            if (res)
                same = true;
            // 5. diff
            res = readInt();
            if (res)
                diff = true;
            _usage = make_pair(same, diff);

            _cmd = nullptr;
            _cmdLen = 0;
        }

        ~ComputeItem() {
            if (_cmdLen > 0)
                free(_cmd);
        }

        void buildCommand(string ckey) {
            _cmd = (char*)calloc(COMPUTE_COMMAND_LEN, sizeof(char));
            _cmdLen = 0;
            _cKey = ckey;

            // 0. dstidx
            writeInt(_dstidx);
            // 1. num srcs
            writeInt(_srclist.size());
            // 2. srclist
            for (int i=0; i<_srclist.size(); i++)
                writeInt(_srclist[i]);
            // 3. coefs
            for (int i=0; i<_coefs.size(); i++)
                writeInt(_coefs[i]);
            // 4. same
            if (_usage.first)
                writeInt(1);
            else
                writeInt(0);
            // 5. diff
            if (_usage.second)
                writeInt(1);
            else
                writeInt(0);
            
        }

        void sendTo(unsigned int ip) {
            redisContext* sendCtx = RedisUtil::createContext(ip);
            redisReply* sendReply = (redisReply*)redisCommand(sendCtx, "RPUSH %s %b", _cKey.c_str(), _cmd, _cmdLen);
            freeReplyObject(sendReply);
            redisFree(sendCtx);
            //cout << "ComputeItem::sendTo.send command len = " << _cmdLen << endl;
        }

        void writeInt(int value) {
            if (_cmdLen + 4 > _maxLen) {
                cout << "ComputeItem::writeInt.extend cmd from length " << _maxLen << " to " << 2*_maxLen << endl;
                _maxLen = _maxLen * 2;    
                char* newcmd = (char*)calloc(_maxLen, sizeof(char));
                memcpy(newcmd, _cmd, _cmdLen);
                free(_cmd); 
                _cmd = newcmd; 
            }

            int tmpv = htonl(value); 
            memcpy(_cmd + _cmdLen, (char*)&tmpv, 4); _cmdLen += 4;
        }

        int readInt() {
            int tmpint;
            memcpy((char*)&tmpint, _cmd + _cmdLen, 4); _cmdLen += 4;
            return ntohl(tmpint);
        }

};

class Task {

    public:
        // 0: Read
        // 1: Send
        // 2: Recv
        // 3: Compute
        // 4: Persist
        int _type;

        int _batch_id;
        int _stripe_id;

        // type 0: Read
        // _blk_name == "ZERO" means that we generate zero packets
        string _blk_name; 
        vector<int> _dagidx_list;
        vector<pair<bool, bool>> _usage_list;

        // type 1: Send
        // _dagidx_list; we should send out these dagidx
        vector<vector<int>> _send_to_list; // each dagidx are send to these nodeids

        // type 2: Recv
        // _dagidx_list

        // type 3: Compute
        vector<ComputeItem*> _compute_list;
        // only used in resolve
        // _dagidx_list;

        // type 4: Persist
        vector<int> _collect_list;
        string _repaired_block;

        // command
        char* _agCmd = 0;
        int _cmLen = 0;
        string _rKey;
        int _maxLen;

        Task(int batchid, int stripeid);
        Task(char* reqStr, int batchid, int stripeid, unsigned int ip);
        ~Task();

        // type: 0
        void buildReadTask(int type, string blockname, vector<int> dagidxlist, vector<pair<bool, bool>> usagelist);
        void buildSendTask(int type, vector<int> dagidxlist, vector<vector<int>> sendtolist);
        void buildRecvTask(int type, vector<int> dagidxlist);
        void buildComputeTask(int type, vector<ComputeItem*> clist);
        void buildPersistTask(int type, vector<int> clist, string blkname);

        void buildTaskCommand();
        void buildReadTaskCommand();
        void buildSendTaskCommand();
        void buildRecvTaskCommand();
        void buildComputeTaskCommand();
        void buildPersistTaskCommand();
        void sendTo(unsigned int ip);

        void resolveReadTask();
        void resolveSendTask();
        void resolveRecvTask();
        void resolveComputeTask(unsigned int ip);
        void resolvePersistTask();

        void writeInt(int value);
        void writeString(string s);
        int readInt(); 
        string readString();

        void dump();
};

#endif
