#ifndef _WORKER_HH_
#define _WORKER_HH_

#include "Config.hh"
#include "DataPacket.hh"
#include "../inc/include.hh"
#include "../protocol/AGCommand.hh"
#include "../ec/Task.hh"
#include "../ec/NodeBatchTask.hh"
#include "../ec/Computation.hh"
#include "../util/BlockingQueue.hh"
#include "../util/DistUtil.hh"

using namespace std;

#define KEY0 -32767

#define debug_read true
#define debug_send true
#define debug_recv true
#define debug_compute true
#define debug_persist true

class ComputeGroup {
    public:
        vector<ComputeItem*> _compute_list;
        unordered_map<int, int> _src_map;

        ComputeGroup() {
        }
        ~ComputeGroup() {
        }

        // check whether ci can be added in this compute group
        bool match(ComputeItem* ci) {
            bool toret = false;
            for (int srcidx: ci->_srclist) {
                if (_src_map.find(srcidx) != _src_map.end()) {
                    toret = true;
                    break;
                }
            }
            return toret;
        }

        void add(ComputeItem* ci) {
            _compute_list.push_back(ci);
            for (int srcidx: ci->_srclist) {
                if (_src_map.find(srcidx) == _src_map.end()) {
                    _src_map.insert(make_pair(srcidx, 1));
                }
            }
        }

        // check whether cg can be merged in this compute group
        bool canMerge(ComputeGroup* cg, int size) {
            int cgsize = cg->_compute_list.size();
            if (_compute_list.size() + cgsize <= size)
                return true;
            else
                return false;
        }

        void merge(ComputeGroup* cg) {
            for (ComputeItem* ci: cg->_compute_list) {
                add(ci);
            }
        }
};

class DataMap {
    public:
        mutex _lock;
        unordered_map<int, BlockingQueue<DataPacket*>*> _datamap;

        DataMap() {
        }

        ~DataMap() {
            for (auto item: _datamap) {
                BlockingQueue<DataPacket*>* queue = item.second;
                if (queue)
                    delete queue;
            }
        }

        void createQueue(int dagidx) {
            BlockingQueue<DataPacket*>* queue = new BlockingQueue<DataPacket*>();
            _lock.lock();
            _datamap.insert(make_pair(dagidx, queue));
            _lock.unlock();
        }
        
        BlockingQueue<DataPacket*>* getQueue(int dagidx) {
            BlockingQueue<DataPacket*>* queue = NULL;
            _lock.lock();
            if (_datamap.find(dagidx) != _datamap.end())
                queue = _datamap[dagidx];
            _lock.unlock();

            return queue;
        }
};

class Worker {
    private:
        Config* _conf;
        redisContext* _processCtx;

        // cache tasks for the current batch
        BlockingQueue<NodeBatchTask*>* _batchtasklist;

        // cache data for the current stripe that we are repairing
        mutex _lock;
        unordered_map<int, BlockingQueue<DataPacket*>*> _datamap;
        unordered_map<int, DataMap*> _stripe2datamap;

        mutex _recv_lock;
        int _recvNum;

        mutex _send_lock;
        int _sendNum;

        mutex _compute_lock;
        int _computeNum;

    public:
        Worker(Config* conf);
        ~Worker();

        void doProcess();

        NodeBatchTask* readInstructions();
        NodeBatchTask* recvBatchTasks(int batchid, int stripenum, vector<int> stripeidlist, vector<int> stripetasknum);

        void repairBatch(NodeBatchTask* nbtask);
        void repairStripe(int batchid, int stripeid, int tasknum, vector<Task*> tasklist);
        void prepareBQ(Task* task);
        void prepareDataMap(Task* task, DataMap* datamap);

        /* READ */
        // read a real block
        void readRealBlock(int batchid, int stripeid, Task* task);
        // generate a zero block
        void readVirtBlock(int batchid, int stripeid, Task* task);

        /* SEND */
        // send sub-blocks one by one
        void sendOneByOne(int batchid, int stripeid, Task* task);
        // send sub-blocks by groups
        void sendGroups(int batchid, int stripeid, Task* task);
        // send mutliple dagidx to a node
        void sendToNode(int batchid, int stripeid, int nodeid, vector<int> dagidxlist);
        // send a dagidx to multiple nodes
        void sendDagidx(int batchid, int stripeid, int dagidx, vector<int> nodeidlist);

        /* RECV */
        // recv sub-blocks one by one
        void recvOneByOne(int batchid, int stripeid, Task* task);
        // recv sub-blocks by groups
        void recvGroups(int batchid, int stripeid, Task* task);
        // recv dagidx list
        void recvList(int batchid, int stripeid, vector<int> dagidx_list);

        /* COMPUTE */
        // compute sub-blocks one by one
        void computeOneByOne(int batchid, int stripeid, Task* task);
        // compute sub-blocks by group
        void computeGroups(int batchid, int stripeid, Task* task);
        // compute a group
        void computeList(int batchid, int stripeid, ComputeGroup* cg);

        /* PERSIST */
        // persist sub-blocks one by one
        void persist(int batchid, int stripeid, Task* task);

        void sendFinishFlag(int batchid);
        double getTimeStamp(struct timeval tv); // in microsecond
        string getTimeString(struct timeval tv);
};

#endif
