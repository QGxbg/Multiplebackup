#include "SolutionBase.hh"

SolutionBase::SolutionBase() {
    _finish_gen_batches = false;
    _batch_request = false;
}

SolutionBase::SolutionBase(string param) {
    // cout << "SolutionBase::param = " << param << endl;
}

void SolutionBase::init(vector<Stripe*> stripe_list, ECBase* ec, string codename, Config* conf) {
    // cout << "SolutionBase::init" << endl;
    _stripe_list = stripe_list;
    _ec = ec;
    _codename = codename;
    _conf = conf;
}

vector<RepairBatch*> SolutionBase::getRepairBatches() {
    return _batch_list;
}

// vector<RepairBatch*> SolutionBase::getRepairBatchesNew() {
//     return _batch_queue;
// }


RepairBatch* SolutionBase::getRepairBatchFromQueue() {
    cout<<"SsolutionBase::getRepairBatchFromQueue start"<<endl;
    if(_batch_queue.getSize()== 0){
	    cout<<"!!!!==0"<<endl;
        _lock.lock();
        _batch_request = true;
        _lock.unlock();
    }
    cout<<"here!"<<endl;
    RepairBatch* toret = _batch_queue.pop();
    cout<<"!!!"<<endl;
    return toret;
}

void SolutionBase::filterFailedStripes(vector<int> fail_node_list) {
    for (int stripeid=0; stripeid < _stripe_list.size(); stripeid++) {
        Stripe* curstripe = _stripe_list[stripeid];
        vector<int> curplacement = curstripe->getPlacement();

        for (int blkid=0; blkid<curplacement.size(); blkid++) {
            int curnode = curplacement[blkid];

            if (find(fail_node_list.begin(), fail_node_list.end(), curnode) == fail_node_list.end())
                continue;
            else if(find(_stripes_to_repair.begin(), _stripes_to_repair.end(), stripeid) == _stripes_to_repair.end())
                _stripes_to_repair.push_back(stripeid);
        }
    }

}

bool SolutionBase::hasNext() {
    if (!_finish_gen_batches) 
        return true;
    else if (_batch_queue.getSize())
        return true;
    else
        return false;
}
