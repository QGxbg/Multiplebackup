#ifndef _AGCOMMAND_HH_
#define _AGCOMMAND_HH_

#include "../inc/include.hh"
#include "../util/RedisUtil.hh"

using namespace std;

/*
 * AGCommand Format
 * ag_request: batch_id|num_stripes|stripe_id1|num_tasks1|stripe_id2|num_tasks2|...
 */

class AGCommand {
  private:
    char* _agCmd = 0;
    int _cmLen = 0;
    string _rKey;

    int _batch_id;
    int _num_stripes;
    vector<int> _stripe_id_list;
    vector<int> _stripe_task_num;

    int _maxLen;

  public:
    AGCommand();
    ~AGCommand();
    AGCommand(char* reqStr);

    // basic construction methods
    void writeInt(int value);
    void writeString(string s);
    int readInt();
    string readString();

    void buildAGCommand(int batchid, int nstripes, vector<int> stripelist, vector<int> numlist);
    void checkLength();

    int getBatchId();
    int getNumStripes();
    vector<int> getStripeIdList();
    vector<int> getStripeTaskNum();

    void sendTo(unsigned int ip);

};

#endif
