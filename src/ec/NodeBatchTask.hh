#ifndef _NODEBATCHTASK_HH_
#define _NODEBATCHTASK_HH_

#include "Task.hh"
#include "../inc/include.hh"
#include "../protocol/AGCommand.hh"

using namespace std;

class NodeBatchTask {

    public:
        int _batch_id;
        vector<int> _stripe_id_list;
        vector<int> _num_list;
        unordered_map<int, vector<Task*>> _taskmap; // stripeid->tasklist
        unsigned int _ip;

        NodeBatchTask(int batchid, vector<int> stripeidlist, vector<int> numlist, unordered_map<int, vector<Task*>> taskmap, unsigned int ip);
        ~NodeBatchTask();

        void sendAGCommand(unsigned int ip);
        void sendTaskCommands(unsigned int ip);
        void sendCommands();
        void waitFinishFlag(int nodeid, unsigned int coorip);

        string getTimeString(struct timeval tv);

};

#endif
