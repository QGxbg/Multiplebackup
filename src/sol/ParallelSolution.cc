#include "ParallelSolution.hh"
#include "OfflineSolution.hh"

ParallelSolution::ParallelSolution(){
    
}

ParallelSolution::ParallelSolution(int batchsize, int standbysize, int agentsnum , int method) {
    _batch_size = batchsize;
    _standby_size = standbysize;
    _agents_num = agentsnum;
    _method = method ; 
    _cluster_size = standbysize + agentsnum;
}

void ParallelSolution::ParallelSolution_standby(int batchsize , int standbysize, int agentsnum) {
    _batch_size = batchsize;
    _standby_size = standbysize;
    _agents_num = agentsnum;
    // LOG << "ParallelSolution::constructor. _batch_size = " << _batch_size << endl;
}

vector<vector<int>> deepCopyTable(const std::vector<std::vector<int>>& source) {
    vector<vector<int>> destination;
    destination.resize(source.size());
    for (size_t i = 0; i < source.size(); ++i) {
        destination[i].resize(source[i].size());
        copy(source[i].begin(), source[i].end(), destination[i].begin());
    }
    return destination;
}

void dumpTable(vector<vector<int>> table)
{
    LOG << "in  ";
    for(auto it : table)
    {
        LOG  << it[1]<<" ";
    }
    LOG << endl;

    LOG << "out ";
    for(auto it : table)
    {
        LOG  << it[0] << " ";
    }
    LOG << endl;
}


void ParallelSolution::genRepairBatches(int num_failures, vector<int> fail_node_list, int num_agents, string scenario, bool enqueue) {
    // We assume that the replacement node ids are the same with the failed ids
    LOG << "ParallelSolution::genRepairBatches begin" << endl;
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;
    // string offline_solution_path = _conf->_tpDir+"/"+_codename+"_"+to_string(ecn)+"_"+to_string(eck)+"_"+to_string(ecw)+".xml";
    // LOG << "offline solution path: " << offline_solution_path << endl;
    // _tp = new TradeoffPoints(offline_solution_path);

    // Xiaolu comment
    // enqueue == true: generated RepairBatch is pushed into a queue in SolutionBase::_batch_queue
    // enqueue == false: generated RepairBatch is added into a vector in SolutionBase::_batch_list
    _enqueue = enqueue;

    if (num_failures == 1) {
        cout<<"genRepairBatchesForSingleFailure start"<<endl;
        genRepairBatchesForSingleFailure(fail_node_list[0], num_agents, scenario);
        
    } else{
        cout<<"genRepairBatchesForMultipleFailure start"<<endl;
        //cout<<fail_node_list.size()<<"  "<<num_agents<<endl;
        //genRepairBatchesForMultipleFailure(fail_node_list, num_agents, scenario , _method);
        genRepairBatchesForMultipleFailureNew(fail_node_list, num_agents, scenario , _method);
        //genRepairBatchesForMultipleFailure
    }

    // Xiaolu comment
    // we have finished generating RepairBatches
    _finish_gen_batches = true;
    


}
//            int ret=hungary(rg_id, cur_match_stripe, _bipartite_matrix, rg_id*_num_stripes_per_group*_num_agents, _node_belong);
int ParallelSolution::hungary(int rg_id, int cur_match_stripe, int* matrix, int matrix_start_addr, int* node_selection) {
    int i;
    int ret;
    for(i=0; i<_num_agents; i++){ // 遍历每个node
        if((matrix[matrix_start_addr + cur_match_stripe*_num_agents + i]==1) && (_mark[i]==0)){ // 该node是该stripe的存储节点
            //LOG << "   hungary " << cur_match_stripe << " check node " << i <<  " node_selection " << " = " << node_selection[rg_id*_num_agents+i] << endl;
            _mark[i]=1;
            int & selection = node_selection[rg_id*_num_agents+i];
            if(node_selection[rg_id*_num_agents+i]==-1){ // 该node尚未匹配
                if (_debug)
                    LOG << "       hungary ret1.1: rg_id " << rg_id << ", mark (" << rg_id << ", " << i << ") = " << cur_match_stripe << endl;
                node_selection[rg_id*_num_agents+i]=cur_match_stripe;
                return 1; 
            } else if ((ret=hungary(rg_id, node_selection[rg_id*_num_agents+i], matrix, matrix_start_addr, node_selection))==1){
                node_selection[rg_id*_num_agents+i] = cur_match_stripe; //尝试调整
                if (_debug)
                    LOG << "       hungary ret1.2: rg_id " << rg_id << ", mark (" << rg_id << ", " << i << ") = " << cur_match_stripe << endl;
                return 1;
            }
        }
    }
    // LOG << "_node_belong:"<<endl;
    // //for(int i = 0 ;i < _stripes_to_repair.size();i++){
    //     for(int k=0; k<_num_agents; k++){
    //         LOG << "" << _node_belong[rg_id * _num_agents + k]<<" ";  
    //     }
    // LOG << endl;
    //}
    
    if (_debug)
        LOG << "hungary: rg_id " << rg_id << ", cur_match_stripe " << cur_match_stripe << ", return 0"<< endl;
    return 0;
}


int ParallelSolution::if_insert(Stripe* repairstripe, int rg_id, int cur_match_stripe, vector<int> soon_to_fail_node) {
    if (_debug) {
        LOG << "if_insert::stripe_id = " << repairstripe->getStripeId() << endl;
        LOG << "if_insert::rg_id = " << rg_id << endl;
        LOG << "if_insert::cur_match_stripe = " << cur_match_stripe << endl;
    }
    // cur_match_stripe 目前已经匹配的stripe数目
    int chunk_id;
    int* bak_node_belong=(int*)malloc(sizeof(int)*_num_agents); // clusterSize大小

    // 0. get out the nodeid of chunks in the repairstripe, and try to set bipartite matrix
    // 得到条带中的每个块所存储的nodeid
    vector<int> nodeList = repairstripe->getPlacement();
    vector<int> helper;
    if (_debug)
        LOG << "nodeids: " ;
    // for (auto chunkinfo: chunkinfos) {
    for (int i = 0; i < nodeList.size(); i++) {
        int chunkidx = i;
        int node_id = nodeList[i];
        if (_debug)
            LOG << node_id << " ";
        if (find(soon_to_fail_node.begin(),soon_to_fail_node.end(),node_id) != soon_to_fail_node.end())
            continue; // nodelist中剔除掉soon to fail node
        helper.push_back(node_id);

        // set bipartite matrix
        // 将该位置置为1  [rg_id][cur_match_stripe][node_id] = 1 非修复节点置1
        _bipartite_matrix[rg_id*_num_stripes_per_group*_num_agents + cur_match_stripe*_num_agents + node_id]=1;
    }
    //LOG << endl;
    //_bipartite_matrix=(int*)malloc(sizeof(int)*_rg_num*_num_stripes_per_group*_num_agents);
    //LOG << "_bipartite_matrix[]:";
    // for(int i = 0 ; i < rg_id; i++){
    //     for(int j = 0;j < cur_match_stripe; j++){
            // for(int k = 0;k < _num_agents;k++){
            //     LOG <<_bipartite_matrix[ cur_match_stripe * _num_agents + k]<< " ";
            // }
            // LOG << endl;
    //     }
    //     LOG << endl;
    // }
    
    // 1. backup node_belong
    for(int k=0; k<_num_agents; k++){
        bak_node_belong[k]=_node_belong[rg_id*_num_agents + k];  
    }


    // 2. use hungary algorithm
    for(chunk_id=0; chunk_id<helper.size(); chunk_id++){            
        memset(_mark, 0, sizeof(int)*_num_agents);
        // 返回0则表示失败
        int ret=hungary(rg_id, cur_match_stripe, _bipartite_matrix, rg_id*_num_stripes_per_group*_num_agents, _node_belong);
        if(ret==0){
            // LOG << "hungary = 0" << endl;
            break;
        } 
    }
    
    // if the repair stripe cannot be inserted into the repair group
    // 匹配失败
    if(chunk_id<=helper.size()-1){
        // LOG << "hungary rd_id = " << rg_id << " match_stripe" << repairstripe->getStripeId() << " false " << endl;
        // reset the bipartite matrix
        for(int node_id=0; node_id<_num_agents; node_id++)
            _bipartite_matrix[rg_id*_num_stripes_per_group*_num_agents+cur_match_stripe*_num_agents+node_id]=0;
        // reset the node belong
        for(int k=0; k<_num_agents; k++)
            _node_belong[rg_id*_num_agents+k]=bak_node_belong[k];
    }

    // return 1: 匹配成功
    // return 0: 匹配失败
    free(bak_node_belong);
    if(chunk_id<=helper.size()-1)
        return 0;
    else
        return 1;
    return 1;
}

void ParallelSolution::update_bipartite_for_replace(int des_id, int stripe_id, int rg_id, int index_in_rg, string flag, vector<int> soon_to_fail_node) {
    // des_id is the index in _collection
    int k;
    // stripe_id is the index in the overall stripes
    int node_id;
    int bi_value;

    if(flag=="delete")
        bi_value=0;
    else
        bi_value=1;

    
    Stripe* repstripe = _stripe_list[des_id]; // 得到des_id所对应的stripe
    // vector<vector<int>> chunkinfos = repstripe->_chunks; // 得到对应的chunkinfo
    vector<int> nodelist = repstripe->getPlacement();
    for(int i = 0;  i < nodelist.size(); i++)
    {
        int chunkidx = i;
        node_id = nodelist[i];
        if (find(soon_to_fail_node.begin(),soon_to_fail_node.end(),node_id) != soon_to_fail_node.end())
            continue;
        // set bipartite matrix
        _bipartite_matrix[rg_id*_num_stripes_per_group*_num_agents+index_in_rg*_num_agents+node_id]=bi_value;
    }
}

int ParallelSolution::replace(int src_id, int des_id, int rg_id, int* addi_id, int num_related_stripes, string flag, vector<int> soon_to_fail_node) {
    if (_debug)
        LOG << "replace src_id = " << src_id << ", des_id = " << des_id << ", rg_id = " << rg_id << endl;

    int src_stripe_id, des_stripe_id;
    int stripe_id;
    int i;
    int j;
    int index;

    int benefit_cnt;
    int* bakp_node_belong=NULL;

    // establish the index of des_id in the _RepairGroup
    // des_id 是被取出的id, 找到该stripe在group中的位置
    for(i=0; i<_cur_matching_stripe[rg_id]; i++)
        if(_RepairGroup[rg_id*_num_stripes_per_group+i] == des_id)
            break; 

    // delete the information of the des_id-th stripe in _bipartite_matrix
    index=i;
    des_stripe_id=_stripes_to_repair[des_id]; // 得到最初的stripe_idx
    update_bipartite_for_replace(des_id, des_stripe_id, rg_id, index, "delete", soon_to_fail_node); // 在二分图中删除该条带
    
    if(flag=="test_replace"){
        bakp_node_belong=(int*)malloc(sizeof(int)*_num_agents);
        memcpy(bakp_node_belong, _node_belong+rg_id*_num_agents, sizeof(int)*_num_agents);                       
    }

        // update the _node_belong information
        for (i=0; i<_num_agents; i++)
            if (_node_belong[rg_id*_num_agents+i]==index)
                _node_belong[rg_id*_num_agents+i]=-1;

    // add the information of the src_id-th stripes in the _bipartite_matrix
    src_stripe_id = _stripes_to_repair[src_id];
    Stripe* src_repair_stripe = _stripe_list[src_stripe_id];
    
    // check if the stripe can be inserted into the stripe
    int ret=if_insert(src_repair_stripe, rg_id, index, soon_to_fail_node);

    if (flag=="test_replace") {
        if(ret == 0 ){
            // reset matrix and nodebelong
            update_bipartite_for_replace(des_id, des_stripe_id, rg_id, index, "add", soon_to_fail_node);
            memcpy(_node_belong+rg_id*_num_agents, bakp_node_belong, sizeof(int)*_num_agents);
            free(bakp_node_belong);
            return 0;
        }

        // calculate the benefit of the replacement 看看这交换能够带来的收益
        benefit_cnt=0;
        int cur_stripe_num;
        cur_stripe_num = _cur_matching_stripe[rg_id];

        // try if other stripes that are not selected can be inserted into the RG 看看是否还能继续插入
        for(i=src_id; i<num_related_stripes; i++){

            if(_ifselect[i]==1) continue;
            if(i==src_id) continue;
        
            stripe_id = _stripes_to_repair[i];
            Stripe* cur_repair_stripe = _stripe_list[stripe_id];
            ret=if_insert(cur_repair_stripe, rg_id, cur_stripe_num, soon_to_fail_node); // 尝试将其他节点插入进去
            if(ret == 1){

                benefit_cnt++; 
                cur_stripe_num++;
                // record the additional stripe id that can be inserted
                j=0;
                while(addi_id[j]!=-1) j++;
                addi_id[j]=i;

            }

            if(cur_stripe_num==_num_stripes_per_group){
                // 到达上限直接退出
                break;
            }
        }

        // reset the _bipartite_matrix and _node_belong
        for(i=rg_id*_num_stripes_per_group*_num_agents; i<(rg_id+1)*_num_stripes_per_group*_num_agents; i++)
            _bipartite_matrix[i]=0;

        for(i=0; i<_cur_matching_stripe[rg_id]; i++){
            stripe_id=_RepairGroup[rg_id*_num_stripes_per_group+i];
            update_bipartite_for_replace(stripe_id, _stripes_to_repair[stripe_id], rg_id, i, "add", soon_to_fail_node);
        }

        memcpy(_node_belong+rg_id*_num_agents, bakp_node_belong, sizeof(int)*_num_agents);
        free(bakp_node_belong);
        return benefit_cnt;
    } else if(flag=="perform_replace"){

        _ifselect[src_id]=1;
        _ifselect[des_id]=0;

        // update _RepairGroup
        i=0;
        while(_RepairGroup[rg_id*_num_stripes_per_group+i]!=des_id) i++;
        _RepairGroup[rg_id*_num_stripes_per_group+i]=src_id;

        i=0;
        while(_record_stripe_id[i]!=-1 && i<_num_stripes_per_group){
            Stripe* repstripe = _stripe_list[_record_stripe_id[i]];
            ret=if_insert(repstripe, rg_id, _cur_matching_stripe[rg_id], soon_to_fail_node);
             
            if(ret==0){                
                printf("ERR-2: if_insert\n");
                exit(1);                                                                                      
            }
            // perform update
            _RepairGroup[rg_id*_num_stripes_per_group + _cur_matching_stripe[rg_id]] = _record_stripe_id[i];
            _cur_matching_stripe[rg_id]++;
            _ifselect[_record_stripe_id[i]]=1;
            i++;
        }
    }

    return 1;
}

int ParallelSolution::greedy_replacement(int num_related_stripes, vector<int> soon_to_fail_node, int rg_id) {
    if (_debug)
        // LOG << "greedy_replacement.numstripes: " << num_related_stripes << ", stfnode: " << soon_to_fail_node << ", rg_id : " << rg_id << endl;
    // 对当前rg进行optimize
    int i;
    int best_src_id, best_des_id;
    int ret;
    int src_id; // 交换对象A    
    int des_id; // 交换对象B
    int max_benefit;
    int if_benefit;

    int* addi_id=(int*)malloc(sizeof(int)*_num_stripes_per_group); // 未知, 大小为group中stripe数量上限
    best_src_id=-1;
    best_des_id=-1;
    if_benefit = 1;
    max_benefit=-1;

    memset(_record_stripe_id, -1, sizeof(int)*_num_stripes_per_group); // 记录当前group中有哪些stripe

    // 如果当前rg中stripes数目等于上限, 即无法再优化
    if(_cur_matching_stripe[rg_id]==_num_stripes_per_group)
        return 0;
    // 遍历所有stripes
    for(src_id=0; src_id<num_related_stripes; src_id++){
        if(_ifselect[src_id]==1)
            continue;
        // 遍历当前已经打包到batch中的stripe
        for(int i=0; i<_cur_matching_stripe[rg_id]; i++){
            memset(addi_id, -1, sizeof(int)*_num_stripes_per_group); // addi_id置为-1, 记录加入到当前group的stripe
            des_id=_RepairGroup[rg_id*_num_stripes_per_group+i]; // 得到当前stripe_id

            string flag = "test_replace"; // 尝试进行交换
            // 交换: 拿出des_id, 插入src_id
            ret = replace(src_id, des_id, rg_id, addi_id, num_related_stripes, flag, soon_to_fail_node);
            if(ret == 0) // 没有增益就继续
                continue;
            if(ret > 0){ // 只要产生增益,就退出对des_id的遍历 
                best_src_id=src_id;
                best_des_id=des_id;
                max_benefit=ret;
                memcpy(_record_stripe_id, addi_id, sizeof(int)*_num_stripes_per_group);
                break;
            }
            if(max_benefit == _num_stripes_per_group - _cur_matching_stripe[rg_id])
                break;
        }
        if(max_benefit == _num_stripes_per_group - _cur_matching_stripe[rg_id])
            break;
    }

    if (_debug)
        LOG << "max_benefit = " << max_benefit << endl;

    // perform replacement
    if(max_benefit !=-1) {
        ret=replace(best_src_id, best_des_id, rg_id, addi_id, num_related_stripes, "perform_replace", soon_to_fail_node);
        if (_debug)
            LOG << "replace res = " << ret << endl;
    } else
        if_benefit = 0;

    if (_debug) {
        LOG << "before exit greedy_replacement" << endl;
        for(int i=0; i<_rg_num; i++){
            for(int j=0; j<_num_stripes_per_group; j++)
                LOG << _RepairGroup[i*_num_stripes_per_group+j] << " ";
            LOG << std::endl;
            
        }
        LOG << "if_benefit = " << if_benefit << endl;
    }

    free(addi_id);
    return if_benefit;
}

vector<RepairBatch*> ParallelSolution::formatReconstructionSets() {
    vector<RepairBatch*> toret;
    for(int i=0; i<_rg_num; i++){
        vector<Stripe*> cursetstripe;
        for(int j=0; j<_num_stripes_per_group; j++){
            int idx = _RepairGroup[i*_num_stripes_per_group+j];
            if (idx >= 0) {
                Stripe* repstripe = _stripe_list[idx];
                cursetstripe.push_back(repstripe);
            }
        }
        if (cursetstripe.size() > 0) {
            RepairBatch* curset = new RepairBatch(i, cursetstripe);
            toret.push_back(curset);
        }
    }

    return toret;
}


vector<RepairBatch*> ParallelSolution::findRepairBatchs(vector<int> soon_to_fail_node, string scenario) {
    LOG << "ParallelSolution::findRepairBatchs start" << endl;
    int i;
    int stripe_id;
    int ret;
    int num_related_stripes = _stripes_to_repair.size();
    _helpers_num = _ec->_n - soon_to_fail_node.size();
    int _expand_ratio = 3;
    int rg_index=0; // 当前选出的group数目
    int flag;

    LOG << "_stripes_to_repair: " ;
    for(auto it : _stripes_to_repair){
        LOG << it << " " ;
    }
    LOG << endl;
    LOG <<  "_num_agents " << _num_agents << endl;
    if (scenario == "scatter") {
        //_num_stripes_per_group = (_num_agents-soon_to_fail_node.size())/(_ec->_n-2); //  -2 ???                      
        _num_stripes_per_group = (_num_agents-soon_to_fail_node.size())/(_ec->_n);  
    } else{
        // TODO
        
        _num_stripes_per_group = (_num_agents-soon_to_fail_node.size())/_ec->_n;
        LOG << "standby " << endl;
        //exit(1);
        // _num_stripes_per_group = (_num_agents-soon_to_fail_node.size())/_ec->_n;
    }
    
    _num_rebuilt_chunks = num_related_stripes;
    _rg_num = (int)(ceil(_num_rebuilt_chunks*1.0/_num_stripes_per_group))*_expand_ratio; // 所能接受的最大group数量
    _RepairGroup=(int*)malloc(sizeof(int)*_rg_num*_num_stripes_per_group);
    _ifselect=(int*)malloc(sizeof(int)*_num_rebuilt_chunks);
    _record_stripe_id=(int*)malloc(sizeof(int)*_num_stripes_per_group);
    _bipartite_matrix=(int*)malloc(sizeof(int)*_rg_num*_num_stripes_per_group*_num_agents);
    _node_belong=(int*)malloc(sizeof(int)*_rg_num*_num_agents);
    _mark=(int*)malloc(sizeof(int)*_num_agents);
    _cur_matching_stripe=(int*)malloc(sizeof(int)*_rg_num);

    LOG << "_num_stripes_per_group = " << _num_stripes_per_group << endl;
    LOG << "_num_rebuilt_chunks = " << _num_rebuilt_chunks << endl;
    LOG << "_rg_num = " << _rg_num << endl;


    // initialization
    int* select_stripe=(int*)malloc(sizeof(int)*_num_stripes_per_group);
    memset(_RepairGroup, -1, sizeof(int)*_rg_num*_num_stripes_per_group);
    memset(_ifselect, 0, sizeof(int)*num_related_stripes);
    memset(_bipartite_matrix, 0, sizeof(int)*_num_stripes_per_group * _num_agents * _rg_num);
    memset(_node_belong, -1, sizeof(int)*_rg_num*_num_agents);
    memset(select_stripe, -1, sizeof(int)*_num_stripes_per_group);
    memset(_cur_matching_stripe, 0, sizeof(int)*_rg_num);

    
    while(true){
        flag = 0;
        // generate an solution for a new repair group
        // 对于第一个group
        for(i=0; i<num_related_stripes; i++){
            // 如果已经挑选过,则跳过
            if(_ifselect[i] == 1)
                continue;
            stripe_id = _stripes_to_repair[i];
            if (_debug)
                LOG << endl << endl <<"findReconstructionSets.stripe_id = " << stripe_id << endl;
            Stripe* repstripe = _stripe_list[stripe_id];
            ret = if_insert(repstripe, rg_index, _cur_matching_stripe[rg_index], soon_to_fail_node);
            if(ret){
                _RepairGroup[rg_index*_num_stripes_per_group + _cur_matching_stripe[rg_index]] = i;
                _ifselect[i]=1;
                _cur_matching_stripe[rg_index]++; // 目前已经匹配到了的stripes数量
                flag=1;
                if(_cur_matching_stripe[rg_index] == _num_stripes_per_group)
                    break;
            }
        }

        if(!flag){
            break;
        } 
    
        // optimize that solution
        ret = 1;
        while(ret)
            ret = greedy_replacement(num_related_stripes, soon_to_fail_node, rg_index);
        rg_index++;
        // assert(rg_index!=_rg_num);
    }

    free(select_stripe);
    vector<RepairBatch*> toret = formatReconstructionSets();
    return toret;
}


// if st1 is better than st2, then return ture;
bool ParallelSolution::isBetter(State st1, State st2)
{
    // if load and bandwidth both equal, will return false
    if(st1._load < st2._load){
        return true;
    }else if(st1._load == st2._load){
        return st1._bdwt < st2._bdwt;
    }
    return false;
}
// if st1 is better than st2, then return ture;
bool ParallelSolution::isBetter2(State st1,int color1, State st2, int color2,const vector<vector<int>> & table)
{
    // just consider global input, because input load is easier to occur imbanlance
    if(st1._load < st2._load){
        return true;
    }else if(st1._load == st2._load){
        if (st1._bdwt < st2._bdwt){
            return true;
        }else{
            //LOG <<""<<endl;
            return false;
            //return (table[color1][0] < table[color2][0]) || (table[color1][0] == table[color2][0]); //全局better
        }
    }
    return false;
}

bool ParallelSolution::isBetter4(State st1,int color1, State st2, int color2,const vector<vector<int>> & table)
{
    // just consider global input, because input load is easier to occur imbanlance
    if(st1._load < st2._load){
        return true;
    }else if(st1._load == st2._load){
        if (st1._bdwt < st2._bdwt){
            return true;
        }else{
            //LOG <<""<<endl;
            return false;
            //return table[color1][0] < table[color2][0]; //全局better
        }
    }
    return false;
}


// if st1 is better than st2, then return ture;
bool ParallelSolution::isBetter3(State st1,int color1, State st2, int color2,const vector<vector<int>> & table)
{
    // just consider input, because input load is easier to occur imbanlance
    if(st1._load < st2._load){
        return true;
    }else if(st1._load == st2._load){
        if (st1._bdwt < st2._bdwt){
            return true;
        }else{
            return table[color1][0] < table[color2][0];
        }

    }
    return false;
}


int ParallelSolution::chooseColor1(Stripe* stripe, vector<int> childColors, unordered_map<int, int> coloring, int idx)
{
    // choose the color which has the minimum local load among the childColors node.
    int bestColor = -1;  
    State bestState(INT_MAX, INT_MAX);
    for(auto newColor : childColors) // 对该节点邻居染色
    {   
        // try new color, and evaluate state
        //LOG<<"newColor:"<<newColor<<endl;
        // cout<<"_num_agents:"<<_num_agents<<endl;
        // cout<<"_agents_num:"<<_agents_num<<endl;
        vector<vector<int>> testTable = stripe->evaluateChange(_num_agents + _standby_size, idx, newColor);
        
        //LOG<<"after newcolor:"<<newColor<<":"<<endl;
        
        //     // 遍历二维向量
        // for (int i = 0; i < testTable.size(); ++i) {
        //     for (int j = 0; j < testTable[i].size(); ++j) {
        //         LOG << "Element at [" << i << "][" << j << "] = " << testTable[i][j] << std::endl;
        //     }
        // }
        
        //State state = evalTable(testTable, childColors);//05 add
        State state = evalTable1(testTable);
        // LOG <<"DEBUG : after change idx :" << idx <<"color : "<<newColor<<endl;
        // LOG <<"load = "<<state._load<<endl;
        // LOG <<"bdwt = "<<state._bdwt<<endl;

        
        if(isBetter3(state, newColor, bestState, bestColor, testTable)){
            bestState = state;
            bestColor = newColor;
        }
    }
    //LOG<<"bestColor choose1 :"<<bestColor<<endl;
    return bestColor;
}
int ParallelSolution::chooseColor2(Stripe* stripe, vector<int> childColors, unordered_map<int, int> coloring, vector<int> itm_idx)
{
    // choose the color which has the minimum local load among the childColors node.
    //LOG << "ParallelSolution::chooseColor2 start" << endl;
    int bestColor = -1;  
    State bestState(INT_MAX, INT_MAX);
    for(auto newColor : childColors) // 对该节点邻居染色
    {   
        // try new color, and evaluate state
       //LOG<<"580 hang newColor:"<<newColor<<endl;
        vector<vector<int>> testTable = stripe->evaluateChange2(_num_agents + _standby_size, itm_idx, newColor);
        

        //State state = evalTable(testTable, childColors);
        State state = evalTable1(testTable);
        // LOG <<"DEBUG chooseColor2: after change idx :" << itm_idx[0] <<" color : "<<newColor<<endl;
        // LOG <<"load = "<<state._load<<endl;
        // LOG <<"bdwt = "<<state._bdwt<<endl;
        // LOG <<"currTable:"<<endl;        
        // dumpTable(testTable);

        if(isBetter(state, bestState)){
            bestState = state;
            bestColor = newColor;
            //LOG<<"bestColor now is :"<<bestColor<<endl;
        }
    }
    //LOG<<"bestColor choose2 :"<<bestColor<<endl;
    return bestColor;
}

int ParallelSolution::chooseColor2New(Stripe* stripe, vector<int> childColors, const vector<vector<int>> & loadTable, vector<int> itm_idx)
{
    // choose the color which has the minimum local load among the childColors node.

    int bestColor = -1;  
    State bestState(INT_MAX, INT_MAX);
    vector<vector<int>> currTable = loadTable;
    // LOG << "685 hang chooseColor2New debug: "<<endl;
    // for(auto it:itm_idx){
    //     LOG << it << " ";
    // }
    // LOG <<endl;


    for(auto newColor : childColors) // 对该节点邻居染色
    {   
        // try new color, and evaluate state
       //LOG<<"683 hang newColor:"<<newColor<<endl;
       currTable = loadTable;
       vector<vector<int>> stripeTable ;
    //    for(auto itm_idx_idx : itm_idx){
    //         stripeTable = stripe->evaluateChange(loadTable.size(), itm_idx_idx, newColor);
    //    }//这个写法有问题，没有加根节点的in

        stripeTable = stripe->evaluateChange2(loadTable.size(), itm_idx, newColor);
        
        //stripeTable = stripe->evaluateChange2(loadTable.size(), itm_idx, newColor); //这个地方有无问题

        //vector<vector<int>> stripeTable = stripe->evaluateChange2(loadTable.size(), itm_idx, newColor);
        
        //LOG<<"stripeTable:"<<endl;
        //dumpTable(stripeTable);

        for(int i = 0; i < loadTable.size(); i++)
        {
            currTable[i][0] += stripeTable[i][0];
            currTable[i][1] += stripeTable[i][1];
        }
        //LOG<<"currTable before evalTable1:"<<endl;
        //dumpTable(currTable);

        //State state = evalTable(currTable, childColors);
        State state = evalTable1(currTable);
        int placement = 0 ;
        for ( int i= 0 ; i < stripe -> _nodelist .size() ; i++){
            if ((stripe -> _nodelist)[i] == newColor){
                placement = i ;
                break;
            } 
        }
        // LOG <<"DEBUG chooseColor2New : after change idx :" << itm_idx[0] <<" color : "<<newColor <<" placement : "<<placement <<endl;
        // LOG <<"load = "<<state._load<<endl;
        // LOG <<"bdwt = "<<state._bdwt<<endl;
        // LOG<<"currTable:"<<endl;
        // dumpTable(currTable);

        if(isBetter2(state, newColor, bestState, bestColor, currTable)){ // gloabal :3 非 ：2
            bestState = state;
            bestColor = newColor;
            //LOG<<"bestColor now is :"<<bestColor<<endl;
        }
    }
    
    return bestColor;
}

int ParallelSolution::chooseColor4(Stripe* stripe,const vector<int> & childColors, const vector<vector<int>> & loadTable, int idx)
{
    // choose the color which leads to minimum global_fulldag load
    int bestColor = -1;
    int minLoad = INT_MAX;  
    State bestState(INT_MAX, INT_MAX);
    for(auto newColor : childColors) // 对该节点邻居染色
    {   
        // try new color, and evaluate state
        
        vector<vector<int>> stripeTable = stripe->evaluateChange(loadTable.size(), idx, newColor);
        vector<vector<int>> currTable = loadTable;
        for(int i = 0; i < loadTable.size(); i++)
        {
            currTable[i][0] += stripeTable[i][0];
            currTable[i][1] += stripeTable[i][1];
        }

        State state = evalTable1(currTable); // eval gloabl load
        if(isBetter3(state, newColor, bestState, bestColor, currTable)){
            bestState = state;
            bestColor = newColor;
        }
    }
    return bestColor;
}


void ParallelSolution::SingleMLP(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring)
{
   LOG << "ParallelSolution::SingleMLP for stripe " << stripe->getStripeId() << endl;

    // 1. init information and blank solution
    vector<int> topoIdxs = ecdag->genTopoIdxs();

    //cout<<"718hang topoIdxs.size():"<<topoIdxs.size()<<endl;
    
    for(auto it : topoIdxs)
    {
        coloring.insert(make_pair(it, -1));
    }
    stripe->setColoring(coloring);
    stripe->evaluateColoring();
    
    // ecdag ->dumpTOPO();

    // for (int i = 0; i < topoIdxs.size(); i++)  //8
    // { cout << topoIdxs[i]<<" ";
    // }
    // cout<<endl;
  
    // 2. coloring the blank node one by one
    for (int i = 0; i < topoIdxs.size(); i++)  //8
    {
        // init global tabl
        
        int idx = topoIdxs[i];
        //LOG<<"color for "<<idx<<endl;

        ECNode* node = ecdag->getECNodeMapNew()[idx];

        int oldColor = coloring[idx];


        // color the vetex with child colors
        vector<int> childColors = node->getChildColors(coloring);

        //reverse(childColors.begin(),childColors.end());
        // int bestColor = chooseColor1(stripe, childColors, coloring, idx);
        int bestColor = chooseColor1(stripe, childColors, coloring, idx);

        if (bestColor == oldColor)
            continue;
        coloring[idx] = bestColor;
        //LOG << "DEBUG : idx "<<idx<<"choose bestColor "<<bestColor<<endl;
        stripe->changeColor(idx, bestColor);
    }
    stripe->evaluateColoring();
    
    unordered_map<int,int>res_color =  stripe->getColoring();
    // for(const pair<int,int>& kv:res_color){
    //     LOG<<kv.first<<"   "<<kv.second<<endl;
    // }
    
    //stripe ->dumpLoad(_num_agents);
    //ecdag->dumpTOPO();
    LOG << "load = " << stripe->getLoad() << endl << "bdwt =  " << stripe->getBdwt() << endl; 
    return;
}

void ParallelSolution::SingleMLPAll(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring,int num_agents)
{
    //LOG << "ParallelSolution::SingleMLP for stripe " << stripe->getStripeId() << endl;

    // 1. init information and blank solution
    vector<int> topoIdxs = ecdag->genTopoIdxs();
    
    for(auto it : topoIdxs)
    {
        coloring.insert(make_pair(it, -1));
    }
    stripe->setColoring(coloring);
    stripe->evaluateColoring();   
  
    // 2. coloring the blank node one by one
    for (int i = 0; i < topoIdxs.size(); i++) 
    {
        // init global tabl
        int idx = topoIdxs[i];
        ECNode* node = ecdag->getECNodeMapNew()[idx];
        int oldColor = coloring[idx];

        vector<int> allcolor;
        
        //JunLong add
        for(int i = 0;i<_num_agents;i++){
             if(find(stripe->_fail_blk_idxs.begin(), stripe->_fail_blk_idxs.end(), i) == stripe->_fail_blk_idxs.end())
                allcolor.push_back(i);
        }
        vector<int> childColors = node->getChildColors(coloring);
        //vector<int> childColors;

        for(int i = 1;i <= stripe->_fail_blk_idxs.size();i++){
            childColors.push_back(_num_agents - i);
        }

        // LOG<<"667 hang allcolor"<<endl;
        // for(auto it : allcolor){
        //     LOG<<it<<" ";
        // }
        std::random_shuffle(allcolor.begin(), allcolor.end());
        std::random_shuffle(childColors.begin(), childColors.end());
        // color the vetex with all colors
        int bestColor = chooseColor1(stripe, childColors, coloring, idx);
        //JunLong add

        // color the vetex with child colors
        //vector<int> childColors = node->getChildColors(coloring);
        //other node color also
        
        //int bestColor = chooseColor1(stripe, childColors, coloring, idx);

        //not only ChildColors

        if (bestColor == oldColor)
            continue;
        coloring[idx] = bestColor;
        stripe->changeColor(idx, bestColor);
    }
    
    stripe->setColoring(coloring);
    stripe->evaluateColoring();

//     cout << "after coloring the intermediate vertex: " << endl;
//   for (auto item: coloring) {
//     cout << "  " << item.first << ": " << item.second << endl;
//   }
    
    // ecdag->dumpTOPO();
    //LOG << "load = " << stripe->getLoad() << endl << "bdwt =  " << stripe->getBdwt() << endl; 
    return;
}

bool ParallelSolution::areAllNodesInLeaves(const std::vector<int>& node_parents, const std::vector<int>& ecdage_leaves) {
    std::unordered_set<int> leaves_set(ecdage_leaves.begin(), ecdage_leaves.end());

    for (int parent : node_parents) {
        if (leaves_set.find(parent) == leaves_set.end()) {
            return false;
        }
    }
    return true;
}

void ParallelSolution::SingleMLPGroup(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring,int num_agents,unordered_map <string,vector<int>>  itm_idx_info)
{
    LOG << "ParallelSolution::SingleMLPGroup for stripe " << stripe->getStripeId() << endl;

    // 1. init information and blank solution
    vector<int> topoIdxs = ecdag->genTopoIdxs();  //中间节点

    
    for(auto it : topoIdxs)
    {
        coloring.insert(make_pair(it, -1));
    }
    stripe->setColoring(coloring);
    stripe->evaluateColoring();   
  
    // 2. coloring the blank node group by group
    for (int i = 0; i < topoIdxs.size(); i++) 
    {
        // init global tabl
        int idx = topoIdxs[i];
        ECNode* node = ecdag->getECNodeMapNew()[idx];
        int oldColor = coloring[idx];

        if(oldColor != -1){   //在上一阶段已经被染色过了
            continue;
        }

        // color the vetex with child colors
        vector<int> childColors = node->getChildColors(coloring);

        // 根据idx 去找 itm_idx
       vector<int> itm_childidx = node->getChildIndices();
        vector<int> itm_idx = ecdag ->getECNodeMapNew()[itm_childidx[0]]->getParentIndices();


            int bestColor = chooseColor2(stripe, childColors, coloring, itm_idx);
            if (bestColor == oldColor)
                continue;

            for(auto it:itm_idx){
                stripe->changeColor(it, bestColor);
                coloring[it] = bestColor;
            }

    }

    
    stripe->evaluateColoring();

    return;
}

void ParallelSolution::SingleMLPLevel(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring,int num_agents,unordered_map <string,vector<int>>  itm_idx_info)
{
    LOG << "ParallelSolution::SingleMLPLevel for stripe " << stripe->getStripeId() << endl;

    // 1. init information and blank solution
    vector<int> topoIdxs = ecdag->genTopoIdxs();  //中间节点

    // LOG<<"783 hang TopoIdxs:"<<endl;
    // for(auto it:topoIdxs){
    //     LOG<<it<<"  ";
    // }
    // LOG<<endl;
    
    for(auto it : topoIdxs)
    {
        coloring.insert(make_pair(it, -1));
    }
    stripe->setColoring(coloring);
    stripe->evaluateColoring();   
  
    // 2. coloring the blank node group by group
    for (int i = 0; i < topoIdxs.size(); i++) 
    {
        // init global tabl
        
        int idx = topoIdxs[i];
        //LOG<<"884hang "<<idx<<endl;
        ECNode* node = ecdag->getECNodeMapNew()[idx];
        int oldColor = coloring[idx];

        if(oldColor != -1){   //在上一阶段已经被染色过了
            //LOG<<"891hang "<<idx<<endl;
            continue;
        }
        //分类型，第一层只传孩子结点颜色
        vector<int> node_child = node->getChildIndices();
        vector<int>ecdage_leaves = ecdag->getECLeaves();

        vector<int>echeaders =  ecdag->getECHeaders();

        // color the vetex with child colors
        vector<int> childColors = node->getChildColors(coloring);

        // 根据idx 去找 itm_idx
       vector<int> itm_childidx = node->getChildIndices();
        vector<int> itm_idx = ecdag ->getECNodeMapNew()[itm_childidx[0]]->getParentIndices();

        // LOG<<"debug:"<<endl;
        // vector<int> debug_cp = ecdag ->getECNodeMap()[8120]->getChildIndices();
        //     for(auto it:debug_cp){
        //         LOG<<it<<" ";
        //     }
        //     LOG<<endl;


        if(areAllNodesInLeaves(node_child,ecdage_leaves)){

            int bestColor = chooseColor2(stripe, childColors, coloring, itm_idx); //New ： global 
            //LOG<<"1004 hang "<<idx<<":"<<bestColor<<endl;
            if (bestColor == oldColor)
                continue;
            
            for(auto it:itm_idx){
                //LOG<<"913hang:"<<it<<endl;
                stripe->changeColor(it, bestColor);
                //LOG <<"AllNodesInLeaves DEBUG : after change idx :" << it <<" color : "<<bestColor<<endl;
                coloring[it] = bestColor;
            }
        }else{

            int bestColor = chooseColor2(stripe, candidates, coloring, itm_idx);
            //LOG<<"1018 hang "<<idx<<":"<<bestColor<<endl;
            if (bestColor == oldColor)
                continue;
            

            for(auto it:itm_idx){
                //LOG<<"936 hang:"<<it<<endl;
                stripe->changeColor(it, bestColor);
                //LOG <<"NoAllNodesInLeaves DEBUG : after change idx :" << it <<" color : "<<bestColor<<endl;
                coloring[it] = bestColor;
            }

        }
        
    }
    
    stripe->evaluateColoring();
    
    unordered_map<int,int>res_color =  stripe->getColoring();
    return;
}

void ParallelSolution::GloballyMLP(Stripe* stripe, const vector<int> & itm_idx, const vector<int> & candidates,ECDAG * ecdag, unordered_map<int, int> & coloring, vector<vector<int>> & 
loadTable)
{
    if(true)
        LOG << "GloballyMLP for stripe " << stripe->getStripeId() << endl;

    // 1. init information and blank solution
    vector<int> topoIdxs = ecdag->genTopoIdxs();
    for(auto it : topoIdxs)
    {
        coloring.insert(make_pair(it, -1));
    }
    stripe->setColoring(coloring);
    stripe->evaluateColoring();   
    // 2. coloring the blank node one by one
    for (int i = 0; i < topoIdxs.size(); i++) 
    {
        // init global tabl
        int idx = topoIdxs[i];
        ECNode* node = ecdag->getECNodeMapNew()[idx];
        int oldColor = coloring[idx];

        // color the vetex with child colors
        vector<int> childColors = node->getChildColors(coloring);
        int bestColor = chooseColor4(stripe, childColors, loadTable, idx);
        if (bestColor == oldColor)
            continue;
        coloring[idx] = bestColor;
        stripe->changeColor(idx, bestColor);
    }
    // 3. add into loadtable
    loadTable = stripe->evalColoringGlobal(loadTable);
    return;
}

void ParallelSolution::GloballyMLP_yh(Stripe* stripe, const vector<int> & itm_idx ,unordered_map<int, int> & coloring, vector<vector<int>> & 
    loadTable)
    {
        LOG << "[DEBUG] GLOBALLY MLP: stripe" << stripe->getStripeId() << endl;
    
        // 1. init information and blank solution
        ECDAG * ecdag = stripe->getECDAG();
        vector<int> topoIdxs = ecdag->genTopoIdxs();
        //ecdag ->dumpTOPO();
        for(auto it : itm_idx)
        {
            coloring.insert(make_pair(it, -1));
            // coloring.insert(make_pair(it, stripe->_new_node));
        }
        stripe->setColoring(coloring);
        stripe->evaluateColoring();

        // LOG<<"before color one by one loadTable:"<<endl;
        // dumpTable(loadTable);
        // 2. coloring the blank node one by one
        for (int i = 0; i < topoIdxs.size(); i++) 
        {
            // init global tabl
            int idx = topoIdxs[i];
            ECNode* node = ecdag->getECNodeMapNew()[idx];
            int oldColor = coloring[idx];
            //cout<< oldColor <<endl;
    
            // color the vetex with child colors
            vector<int> childColors = node->getChildColors(coloring);
            
            int bestColor;
            if(childColors.size() == 1 && childColors[0] == -1)
            {
                bestColor = -1;
            }else {
                bestColor = chooseColor_fullnode(stripe, childColors, loadTable, idx);             
            }
            if (bestColor == oldColor)
                continue;
            coloring[idx] = bestColor;
            // LOG << "DEBUG : idx "<<idx<<" choose bestColor "<<bestColor<<endl;
            // dumpTable(loadTable);
            stripe->changeColor(idx, bestColor);      
        }
        
        // 3. add into loadtable

        //dumpTable(loadTable);

        loadTable = stripe->evalColoringGlobal(loadTable);
        //dumpTable(loadTable);
        stripe->evaluateColoring();  
        return;
    }
    
int ParallelSolution::chooseColor_fullnode(Stripe* stripe,const vector<int> & childColors, const vector<vector<int>> & loadTable, int idx)
{
        // choose the color which leads to minimum global_load
    int bestColor = -1;
    int minLoad = INT_MAX;  
    State bestState(INT_MAX, INT_MAX);
        
    for(auto newColor : childColors) 
    {   
        // try new color, and evaluate state
        vector<vector<int>> stripeTable = stripe->evaluateChange(loadTable.size(), idx, newColor);
        // LOG<<"newcolor : "<<newColor << " , stripeTable:"<<endl;
        // dumpTable(stripeTable);

        vector<vector<int>> currTable = loadTable;
        for(int i = 0; i < loadTable.size(); i++)
        {
            currTable[i][0] += stripeTable[i][0];
            currTable[i][1] += stripeTable[i][1];
        }
        // LOG<<"currTable:"<<endl;
        // dumpTable(currTable);
    
        State state = evalTable1(currTable); // eval gloabl load
        // LOG <<"DEBUG chooseColor_fullnode: after change idx :" << idx <<"color : "<<newColor<<endl;
        // LOG <<"load = "<<state._load<<endl;
        // LOG <<"bdwt = "<<state._bdwt<<endl;
        if(isBetter2(state, newColor, bestState, bestColor, currTable)){
            bestState = state;
            bestColor = newColor;
        }
    }
    return bestColor;
}

void ParallelSolution::genParallelColoringForSingleFailure(Stripe* stripe, 
        int fail_node_id, int num_agents, string scenario,  vector<vector<int>> & loadTable) {
    // map a sub-packet idx to a real physical node id
    LOG << "ParallelSolution::genParallelColoringForSingleFailure start" << endl;
    ECDAG* ecdag = stripe->getECDAG();
    unordered_map<int, int> res;
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    int fail_block_idx = -1;
    for (int i=0; i<curplacement.size(); i++) {
        if (curplacement[i] == fail_node_id)
            fail_block_idx = i;
    }

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    //LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1
    int realLeaves = 0;
    vector<int> avoid_node_ids;
    avoid_node_ids.push_back(fail_node_id);
    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;
        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }


    // choose new node after coloring
    int repair_node_id;
    vector<int> candidates;
    if (scenario == "standby")
        repair_node_id = fail_node_id;
    else {
        repair_node_id = -1;

        // remove source nodes that we should avoid 
        for (int i=0; i<num_agents; i++) {
            if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                candidates.push_back(i);
        }
        
        // choose the minimum inputLoad node as new_node
        int minLoad = INT_MAX;
        for(auto it : candidates)
        {
            int inload = loadTable[it][1];
            if(inload < minLoad)
            {
                minLoad = inload;
                repair_node_id = it;
            }
        }
    }
    stripe->_new_node = repair_node_id;
    res.insert(make_pair(ecHeaders[0], repair_node_id));

    // intermediate node idxs
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());    
    struct timeval time1, time2;
    GloballyMLP(stripe ,itm_idx, candidates, ecdag, res, loadTable);
}
// TODO: multiple ecdag coloring
void ParallelSolution::genColoringForMultipleFailure(Stripe* stripe, unordered_map<int, int>& res, vector<int> fail_node_ids, int num_agents, string scenario, vector<int> & placement,int greedy) 
{
    // map a sub-packet idx to a real physical node id
    LOG << "ParallelSolution::genColoringForMultipleFailure start" << endl;
    //assert(_ec->_n + fail_node_ids.size() <= num_agents);
    _num_agents = num_agents;
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    vector<int> fail_block_idx;
    for (int i=0; i<curplacement.size(); i++) {
        for(auto fail_node_id: fail_node_ids) {
            if (curplacement[i] == fail_node_id)
                fail_block_idx.push_back(i);
        }
    }
    cout<<"1152 hang"<<endl;

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    // LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1
    int realLeaves = 0;
    vector<int> avoid_node_ids = fail_node_ids;
    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;

        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
                avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }


    // choose new node after coloring
    int repair_node_id;
    vector<int> candidates;
    if (scenario == "standby"){
        LOG << "1216 hang  ParallelSolution::genColoringForMultipleFailure start!"<<endl;
        //exit(1);
        // for each stripe
        int failnum_count = 0;
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            

            int idx = stripe -> getStripeId() % _standby_size;
            repair_node_id =  _agents_num +  failnum_count ;//yuhan

            // random choose the replacement node for failblock
            //repair_node_id = fail_node_ids[failnum_count];
            res.insert(make_pair(concact_idx, repair_node_id));
            failnum_count ++ ;
            LOG << "1318 hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }
    } 
    else {
            cout<<"1186 hang"<<endl;
        // for each stripe
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            for (int i=0; i<num_agents; i++) {
                if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                    candidates.push_back(i);
            }
            // random choose the replacement node for failblock
            repair_node_id = candidates[rand() % candidates.size()];
            avoid_node_ids.push_back(repair_node_id);
            res.insert(make_pair(concact_idx, repair_node_id));
            // LOG << "DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }
    }
        cout<<"1202 hang"<<endl;
    // intermediate node idxs
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());    


    //    LOG<<"1083 hang itm_idx:parent"<<endl;
    // //分层去染色
    // for(auto it:itm_idx){
    //     vector<int> parent = ecNodeMap[it]->getChildIndices();  //往下看的 根 32767 32766
    //     LOG<<it<<"  :  " ;
    //     for(auto it_p:parent){
    //         LOG<<it_p<<"   ";
    //     }
    //     LOG<<endl;
    // }

    struct timeval time1, time2;
    //SingleMLP(stripe,itm_idx, candidates, ecdag, res);
    cout<<"1228 hang"<<endl;
    if(greedy){
        SingleMLPAll(stripe,itm_idx, candidates, ecdag, res,_num_agents);
    }   else{
        SingleMLP(stripe,itm_idx, candidates, ecdag, res);
    } 
    // // debug for coloring
    // if(DEBUG_ENABLE){
    //     LOG << "coloring result" << endl;
    //     for(auto it : res){
    //         LOG << it.first << " " << it.second << endl;
    //     }
    // }
    
}

// TODO: multiple ecdag coloring for level
void ParallelSolution::genColoringForMultipleFailureLevel(Stripe* stripe, unordered_map<int, int>& res, vector<int> fail_node_ids, int num_agents, string scenario, vector<int> & placement,int greedy) 
{
    // map a sub-packet idx to a real physical node id
    //LOG << "ParallelSolution::genColoringForMultipleFailureLevel start" << endl;
    //assert(_ec->_n + fail_node_ids.size() <= num_agents);
    _num_agents = num_agents;
    
    //cout<<"1247 hang:"<<num_agents<<endl;
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();

    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    vector<int> fail_block_idx;
    for (int i=0; i<curplacement.size(); i++) {
        for(auto fail_node_id: fail_node_ids) {
            if (curplacement[i] == fail_node_id)
                fail_block_idx.push_back(i);
        }
    }

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    // LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1
    int realLeaves = 0;
    vector<int> avoid_node_ids = fail_node_ids;

    int ecq = ecn - eck ;
    int ect =  log(ecw) / log(ecq);
    int ecnu =  (ecn - eck) * ect - ecn;

    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;

        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
                avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }else{
            // nodeid = curplacement[blkidx - ecnu];
            // if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
            //     avoid_node_ids.push_back(nodeid);
            // realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }
    cout<<"1440 hang avoid_node_ids:"<<endl;  //4
    for(auto it:avoid_node_ids){
        cout<<it<<" ";
    }
    cout<<endl;


    vector<int> candidates_scatter;

    // for (int i=0; i<num_agents; i++) {
    //     if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
    //         candidates_scatter.push_back(i);
    // }
    // LOG<<"ecLeaves color:"<<endl;
    // for (auto dagidx: ecLeaves) {
    //     LOG<<dagidx<<" : "<<res[dagidx]<<endl;
    // }

    // choose new node after coloring
    int repair_node_id;
    vector <int> repair_node_id_concact ;
    vector<int> candidates;
    if (scenario == "standby"){
        //LOG << "1463 hang  ParallelSolution::genColoringForMultipleFailureLevel start!"<<endl;
        
        
        // TODO: 
        // for each stripe
        int fail_node_count = 0 ;
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            // for (int i=0; i<num_agents; i++) {
            //     if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
            //         candidates.push_back(i);
            // }
            //cout<<"concact_idx"<<concact_idx<<endl;
            //cout<<candidates.size()<<endl;
            //int idx = stripe->getStripeId % _standby_size;

            int idx = stripe -> getStripeId() % _standby_size;
            repair_node_id =  _agents_num +  fail_node_count ;//yuhan

            //相同的curplacement[i]的repair_node_id相同
            //repair_node_id =  fail_node_ids[fail_node_count];
            fail_node_count ++;

            res.insert(make_pair(concact_idx, repair_node_id));
            //LOG << "1484hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }
    } 
    else {
        _standby_size = 0 ;
        // for each stripe
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            for (int i=0; i<num_agents; i++) {
                if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                    candidates.push_back(i);
            }
            cout<<"concact_idx"<<concact_idx<<endl;
            //cout<<candidates.size()<<endl;
            // random choose the replacement node for failblock
            repair_node_id = candidates[rand() % candidates.size()];
            avoid_node_ids.push_back(repair_node_id);
            //repair_node_id_concact.push_back(repair_node_id);
            res.insert(make_pair(concact_idx, repair_node_id));
            //LOG << "1505 hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
            //cout << "[INFO] stripe " << stripe->getStripeId() << " fail nodeidx is " << (32767-concact_idx)<< " choose new node " << repair_node_id <<"scatter"<<endl;
            
        }
    }

    cout<<"avoid_node_ids:"<<endl;  //4
    for(auto it:avoid_node_ids){
        cout<<it<<" ";
    }
    cout<<endl;

    // intermediate node idxs
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());
    
    
    cout<<"ecHeaders size : "<<ecHeaders.size()<<endl;
    cout<<"itm_idx size : "<<itm_idx.size()<<endl;
    cout<<"leaves size : "<<ecLeaves.size()<<endl;

    struct timeval time1, time2;

//    LOG<<"1083 hang itm_idx:parent"<<endl;
//     //分层去染色
//     for(auto it:itm_idx){
//         vector<int> parent = ecNodeMap[it]->getParentIndices();
//         LOG<<it<<"  :  " ;
//         for(auto it_p:parent){
//             LOG<<it_p<<"   ";
//         }
//         LOG<<endl;
//     }

    //0. 分组

    unordered_map <string,vector<int>>  itm_idx_info;
    for(auto it : itm_idx){
        //LOG<<it<<endl;
        vector<int> itm_childidx = ecNodeMap[it] -> getChildIndices();
        //转换为string
        sort(itm_childidx.begin(),itm_childidx.end());
        string itm_child ;
        for (int num : itm_childidx) {
        itm_child += std::to_string(num);
        }
        //LOG<<itm_child<<endl;
        vector<int> itm_parentidx = ecNodeMap[itm_childidx[0]]->getParentIndices();
        itm_idx_info.emplace(itm_child,itm_parentidx);
    }
    cout<<endl;


    vector<int> candidates_color;
    //cout << "1563 hang candidates_color:num_agents,"<<num_agents<<" _standby_size: "<<_standby_size<<endl;

    for (int i=0; i < num_agents + _standby_size; i++) {
        if (find(fail_node_ids.begin(), fail_node_ids.end(), i) == fail_node_ids.end()){
            if(find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end()){
                candidates_color.push_back(i);
            }
        }

    }
    candidates.clear();
    for (int i=0; i < num_agents; i++) {
        if (find(fail_node_ids.begin(), fail_node_ids.end(), i) == fail_node_ids.end()){
            //if(find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end()){
                candidates.push_back(i);
            //}
        }
    }
    //1.分组染色

        // LOG<<"1531 hang candidates:"<<endl;
        // for(auto it_p:candidates){
        //     LOG<<it_p<<"   ";
        // }
        // LOG<<endl;

    if (scenario == "standby"){
            //SingleMLP(stripe,itm_idx, candidates, ecdag, res);
        if(greedy == 1){
            //SingleMLPAll(stripe,itm_idx, candidates, ecdag, res,_num_agents);
            SingleMLPLevel(stripe,itm_idx, candidates_color, ecdag, res,_num_agents,itm_idx_info);
            //GloballyMLPLevel(stripe,itm_idx,res,candidates_color,loadTable);
        }   else if(greedy == 0){
            SingleMLP(stripe,itm_idx, candidates_color, ecdag, res);
        }  else if(greedy == 2){
            SingleMLPGroup(stripe,itm_idx, candidates_color, ecdag, res,_num_agents,itm_idx_info);
        }
    }else{
                //SingleMLP(stripe,itm_idx, candidates, ecdag, res);
        if(greedy == 1){
            //SingleMLPAll(stripe,itm_idx, candidates, ecdag, res,_num_agents);
            SingleMLPLevel(stripe,itm_idx, candidates, ecdag, res,_num_agents,itm_idx_info);
        }   else if(greedy == 0){
            SingleMLP(stripe,itm_idx, candidates, ecdag, res);
        }  else if(greedy == 2){
            SingleMLPGroup(stripe,itm_idx, candidates, ecdag, res,_num_agents,itm_idx_info);
        }

    }
}

// TODO: multiple ecdag coloring for level
void ParallelSolution::genColoringForMultipleFailureLevelGlobal(Stripe* stripe, unordered_map<int, int>& res, vector<int> fail_node_ids, string scenario,vector<vector<int>> & loadTable,int method) 
{
    // map a sub-packet idx to a real physical node id
    //LOG << "ParallelSolution::genColoringForMultipleFailureLevel start" << endl;
    //assert(_ec->_n + fail_node_ids.size() <= num_agents);
    
    //cout<<"1247 hang:"<<num_agents<<endl;
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();

    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    vector<int> fail_block_idx;
    for (int i=0; i<curplacement.size(); i++) {
        for(auto fail_node_id: fail_node_ids) {
            if (curplacement[i] == fail_node_id)
                fail_block_idx.push_back(i);
        }
    }

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    // LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1
    int realLeaves = 0;
    vector<int> avoid_node_ids = fail_node_ids;

    int ecq = ecn - eck ;
    int ect =  log(ecw) / log(ecq);
    int ecnu =  (ecn - eck) * ect - ecn;

    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;

        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
                avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }else{
            // nodeid = curplacement[blkidx - ecnu];
            // if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
            //     avoid_node_ids.push_back(nodeid);
            // realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }

    cout<<"avoid_node_ids:"<<endl;  //4
    for(auto it:avoid_node_ids){
        cout<<it<<" ";
    }
    cout<<endl;

    vector<int> candidates_scatter;

    // choose new node after coloring
    int repair_node_id;
    vector <int> repair_node_id_concact ;
    vector<int> candidates;
    if (scenario == "standby"){
        LOG << "1687 hang  ParallelSolution::genColoringForMultipleFailureLevelGlobal standby start!"<<endl;
        
        
        // TODO: 
        // for each stripe
        int fail_node_count = 0 ;
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            // for (int i=0; i<num_agents; i++) {
            //     if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
            //         candidates.push_back(i);
            // }
            //cout<<"concact_idx"<<concact_idx<<endl;
            //cout<<candidates.size()<<endl;
            //int idx = stripe->getStripeId % _standby_size;

            int idx = stripe -> getStripeId() % _standby_size;
            repair_node_id =  _agents_num +  fail_node_count ;//yuhan

            //相同的curplacement[i]的repair_node_id相同
            //repair_node_id =  fail_node_ids[fail_node_count];
            fail_node_count ++;
            repair_node_id_concact.push_back(repair_node_id);

            res.insert(make_pair(concact_idx, repair_node_id));
            LOG << "1716 hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }
    } 
    else {
        _standby_size = 0 ;
        // for each stripe
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            for (int i=0; i <_agents_num; i++) {
                if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                    candidates.push_back(i);
            }
            cout<<"concact_idx"<<concact_idx<<endl;
            //cout<<candidates.size()<<endl;
            // random choose the replacement node for failblock
            repair_node_id = candidates[rand() % candidates.size()];
            avoid_node_ids.push_back(repair_node_id);
            //repair_node_id_concact.push_back(repair_node_id);
            res.insert(make_pair(concact_idx, repair_node_id));
            LOG << "1737 hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }
    }
    // intermediate node idxs
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());
    
    
    cout<<"ecHeaders size : "<<ecHeaders.size()<<endl;
    cout<<"itm_idx size : "<<itm_idx.size()<<endl;
    cout<<"leaves size : "<<ecLeaves.size()<<endl;

    struct timeval time1, time2;

    //分层去染色
    //0. 分组

    unordered_map <string,vector<int>>  itm_idx_info;
    for(auto it : itm_idx){
        //LOG<<it<<endl;
        vector<int> itm_childidx = ecNodeMap[it] -> getChildIndices();
        //转换为string
        sort(itm_childidx.begin(),itm_childidx.end());
        string itm_child ;
        for (int num : itm_childidx) {
        itm_child += std::to_string(num);
        }
        //LOG<<itm_child<<endl;
        vector<int> itm_parentidx = ecNodeMap[itm_childidx[0]]->getParentIndices();
        itm_idx_info.emplace(itm_child,itm_parentidx);
    }
    cout<<endl;


    vector<int> candidates_color;
    cout << "1777 hang candidates_color:num_agents,"<<_agents_num<<" _standby_size: "<<_standby_size<<endl;

    // //standby 候选颜色
    // for (int i=0; i < _cluster_size; i++) {
    //     if (find(fail_node_ids.begin(), fail_node_ids.end(), i) == fail_node_ids.end()){
    //         //if(find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end()){
    //             candidates_color.push_back(i);
    //         //}
    //     }
    // }
    candidates_color = repair_node_id_concact;

    //scatter 候选颜色
    candidates.clear();
    for (int i=0; i < _cluster_size; i++) {
        if (find(fail_node_ids.begin(), fail_node_ids.end(), i) == fail_node_ids.end()){
            //if(find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end()){
                candidates.push_back(i);
            //}
        }
    }
    //1.分组染色

    if (scenario == "standby"){
            //SingleMLP(stripe,itm_idx, candidates, ecdag, res);
        if(method == 1){
            //SingleMLPAll(stripe,itm_idx, candidates, ecdag, res,_num_agents);
            //SingleMLPLevel(stripe,itm_idx, candidates_color, ecdag, res,_num_agents,itm_idx_info);
            GloballyMLPLevel(stripe,itm_idx,res,candidates_color,loadTable);
        }   else if(method == 0){
            SingleMLP(stripe,itm_idx, candidates_color, ecdag, res);
        }  else if(method == 2){
            SingleMLPGroup(stripe,itm_idx, candidates_color, ecdag, res,_num_agents,itm_idx_info);
        }
    }else{
                //SingleMLP(stripe,itm_idx, candidates, ecdag, res);
        if(method == 1){
            //SingleMLPAll(stripe,itm_idx, candidates, ecdag, res,_num_agents);
            SingleMLPLevel(stripe,itm_idx, candidates, ecdag, res,_num_agents,itm_idx_info);
        }   else if(method == 0){
            SingleMLP(stripe,itm_idx, candidates, ecdag, res);
        }  else if(method == 2){
            SingleMLPGroup(stripe,itm_idx, candidates, ecdag, res,_num_agents,itm_idx_info);
        }

    }
}


void ParallelSolution::genColoringForMultipleFailureLevelNew(Stripe* stripe,vector<int> fail_node_ids,string scenario,vector<vector<int>> & loadTable,int method) 
{
    assert(loadTable.size() == _cluster_size);
    unordered_map<int,int>coloring;
    vector<int> avoid_node_ids;
    prepare(stripe, fail_node_ids, coloring, scenario, loadTable);
    for (auto item: coloring) {
        int dagidx = item.first;
        int mycolor = item.second;
        //LOG<<"1890  hang dagidx :"<<dagidx<<" , coloring"<<mycolor<<endl;
    }
    ECDAG* ecdag = stripe->getECDAG();
    //vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> itm_idx = ecdag->genItmIdxs();
    stripe->dumpPlacement();

    LOG << stripe ->_nodelist.size()<<endl;

    vector<int> candidates;
    // for (int i=0; i < _cluster_size; i++) { //所有可用节点的
    //     if (find(fail_node_ids.begin(), fail_node_ids.end(), i) == fail_node_ids.end()){
    //         if(candidates.size() < stripe ->_nodelist.size())   //怀疑是agent 数量
    //             candidates.push_back(i); //你这样的话就全是小的了，含没有参与进来的 后面的agent
    //     }
    // }

    for(int i = 0 ; i < stripe ->_nodelist.size(); i++){
        if (find(fail_node_ids.begin(), fail_node_ids.end(), stripe ->_nodelist[i]) == fail_node_ids.end()){
                candidates.push_back(stripe ->_nodelist[i]); //幸存节点
        }
    }
    //res.insert(make_pair(concact_idx, repair_node_id));
    for(int i = 0 ; i < stripe -> getFailNum(); i ++){
        candidates.push_back(coloring[stripe ->getECDAG()->_ecHeaders[i]]);
    }




    // LOG << "1880 HANG candidates：";
    // for(int i = 0 ;i < candidates.size(); i++ ){
    //     LOG << candidates[i]<<" ";
    // }
    // LOG << endl;

    // for(int i = 0 ; i < (ecdag ->_ecHeaders).size(); i++){//仅根节点的，验证一下
    //     candidates.push_back(coloring[ecdag ->_ecHeaders[i]]);
    // }



    //reverse(candidates.begin(),candidates.end()); //优先采用根节点颜色
    // dumpTable(loadTable);

    if(method==1){ // TODO : 根据故障数进行分类
        GloballyMLPLevel(stripe ,itm_idx, coloring,candidates,loadTable);
    }else{
        GloballyMLP_yh(stripe ,itm_idx, coloring,loadTable);
    }
    stripe -> setColoring(coloring);
    stripe -> evaluateColoring();

    // for(auto it:coloring){
    //     LOG<<it.first<<":"<<it.second<<endl;
    // }
}


void ParallelSolution::GloballyMLPLevel(Stripe* stripe, const vector<int> & itm_idx ,unordered_map<int, int> & coloring, const vector<int> & candidates,vector<vector<int>> & 
    loadTable)
    {
        LOG << "[DEBUG] GloballyMLPLevel MLP: stripe " << stripe->getStripeId() << endl;
    
        // 1. init information and blank solution
        ECDAG * ecdag = stripe->getECDAG();
        vector<int> topoIdxs = ecdag->genTopoIdxs();
        //ecdag ->dumpTOPO ();
        
        for(auto it : itm_idx)
        {
            coloring.insert(make_pair(it, -1));
            // coloring.insert(make_pair(it, stripe->_new_node));
        }
        
        //cout<<"itm_idx : "<<itm_idx.size()<<endl;

        //cout<<coloring.size()<<endl;
        stripe->setColoring(coloring);
        stripe->evaluateColoring(); //有问题
        for (auto item: coloring) {
            int dagidx = item.first;
            int mycolor = item.second;
            //LOG<<"1890  hang dagidx :"<<dagidx<<" , coloring"<<mycolor<<endl;
        }

        //stripe ->dumpLoad(_cluster_size); //这里就64了？
    
        //cout << "000 <<<<<<<<<<<<<<< " << endl;
        //dumpTable(loadTable);
        // 2. coloring the blank node one by one
        for (int i = 0; i < topoIdxs.size(); i++) 
        {
            // init global tabl
            int idx = topoIdxs[i];
            ECNode* node = ecdag->getECNodeMapNew()[idx];
            int oldColor = coloring[idx];
    
            if(oldColor != -1){   //在上一阶段已经被染色过了
                //cout<<"[info]1606 hang idx"<<idx<<"has been colored : "<< oldColor <<endl;
                continue;
            }
            //分类型，第一层只传孩子结点颜色
            vector<int> node_child = node->getChildIndices();
            vector<int> ecdage_leaves = ecdag->getECLeaves();
    
            // color the vetex with child colors
            vector<int> childColors = node->getChildColors(coloring);

            //candidates传过来根节点的颜色
            vector<int> candidates_coloring = candidates ;
            //candidates_coloring.insert(candidates_coloring.end(),childColors.begin(),childColors.end()); //应该是只要根节点和孩子节点颜色，不需要那么多
            
            //reverse(childColors.begin(),childColors.end());
    
            // 根据idx 去找 itm_idx
            vector<int> itm_childidx = node->getChildIndices();
            vector<int> itm_idx_parents = ecdag ->getECNodeMapNew()[itm_childidx[0]]->getParentIndices();
    
            // LOG << "before coloring loadTable:"<<endl;
            // dumpTable(loadTable);
            
            if(areAllNodesInLeaves(node_child,ecdage_leaves)){//第一层
                
                // if(idx == 6611){
                //     LOG << "idx 6611 childColors:";
                //     for(auto it : childColors){
                //         LOG << it <<" ";
                //     }
                //     LOG<<endl;
                // }
                int bestColor = chooseColor2New(stripe, childColors, loadTable, itm_idx_parents);
                //LOG<<"1969 hang "<<idx<<":"<<bestColor<<endl;

                if (bestColor == oldColor)
                    continue;
                for(auto it:itm_idx_parents){
                    stripe->changeColor(it, bestColor);
                    coloring[it] = bestColor;
                    //LOG <<"AllNodesInLeaves DEBUG : after change idx :" << it <<" color : "<<bestColor<<endl;
                    // LOG <<"load = "<<state._load<<endl;
                    // LOG <<"bdwt = "<<state._bdwt<<endl;
                }
            }else{
                int bestColor = chooseColor2New(stripe, candidates_coloring, loadTable, itm_idx_parents);
                //LOG<<"1982 hang "<<idx<<":"<<bestColor<<endl;
                int placement ;
                for ( int i= 0 ; i < stripe -> _nodelist .size() ; i++){
                    if ((stripe -> _nodelist)[i] == bestColor){
                        placement = i ;
                        break;
                    } 
                }
                if (bestColor == oldColor)
                    continue;
                for(auto it:itm_idx_parents){
                    stripe->changeColor(it, bestColor);
                    coloring[it] = bestColor;
                    //LOG <<"NoAllNodesInLeaves DEBUG : after change idx :" << it <<" color : "<<bestColor<<" placement : "<<placement<<endl;
                }
            }
        }
        // 3. add into loadtable
        dumpTable(loadTable);
        loadTable = stripe->evalColoringGlobal(loadTable);
        //dumpTable(loadTable);
        stripe->evaluateColoring();
        return;
    }


void ParallelSolution::genParallelColoringForMultipleFailure(Stripe* stripe, 
        vector<int> fail_node_ids, int num_agents, string scenario,  vector<vector<int>> & loadTable) {
    // map a sub-packet idx to a real physical node id
    // LOG << "ParallelSolution::genParallelColoringForMultipleFailure start" << endl;
    _num_agents = num_agents;
    ECDAG* ecdag = stripe->getECDAG();
    unordered_map<int, int> res;
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    vector<int> fail_block_idx;
    for (int i=0; i<curplacement.size(); i++) {
        for(auto fail_node_id: fail_node_ids) {
            if (curplacement[i] == fail_node_id)
                fail_block_idx.push_back(i);
        }
    }



    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    //LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1
    int realLeaves = 0;
    vector<int> avoid_node_ids = fail_node_ids;

    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;
        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
                avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }


    // choose new node after coloring
    int repair_node_id;
    vector<int> candidates;
    if (scenario == "standby"){
        // TODO:
        //exit(1);
        //repair_node_id 做匹配   single:repair_node_id = fail_node_id;

        LOG << "1668 hang  ParallelSolution::genParallelColoringForMultipleFailure start!"<<endl;
        
        int fail_node_count = 0 ;
        // TODO: 
        // for each stripe
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            // for (int i=0; i<num_agents; i++) {
            //     if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
            //         candidates.push_back(i);
            // }
            cout<<"concact_idx"<<concact_idx<<endl;
            //cout<<candidates.size()<<endl;
            //int idx = stripe->getStripeId % _standby_size;

            int idx = stripe -> getStripeId() % _standby_size;
            repair_node_id =  _agents_num +  fail_node_count ;//yuhan

            //相同的curplacement[i]的repair_node_id相同
            //repair_node_id =  fail_node_ids[fail_node_count];
            fail_node_count ++;

            res.insert(make_pair(concact_idx, repair_node_id));
            LOG << "2047 hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }

    }
        
    else {
        // for each stripe
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            for (int i=0; i<num_agents; i++) {
                if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                    candidates.push_back(i);
            }
            // random choose the replacement node for failblock
            repair_node_id = candidates[rand() % candidates.size()];
            avoid_node_ids.push_back(repair_node_id);
            res.insert(make_pair(concact_idx, repair_node_id));
            if(DEBUG_ENABLE)
                LOG << "2067 hang DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
        }
    }


    // intermediate node idxs
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());    
    struct timeval time1, time2;
    GloballyMLP(stripe ,itm_idx, candidates, ecdag, res, loadTable);  
    // if(true){
    //     LOG << "parallel coloring result" << endl;
    //     for(auto it : res){
    //         LOG << it.first << " " << it.second << endl;
    //     }
    // }
    // ecdag ->dumpTOPO();

    //stripe ->dumpLoad (num_agents);
}


void ParallelSolution::genOfflineColoringForSingleFailure(Stripe* stripe, 
        int fail_node_id, int num_agents, string scenario,vector<vector<int>> & loadTable, vector<int> & placement) {
    // choose a new node base on placement
    // add stripe_load on loadTable
    LOG << "ParallelSolution::genOfflineColoringForSingleFailure start" << endl;
    ECDAG* ecdag = stripe->getECDAG();
    unordered_map<int, int> res;
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    int fail_block_idx = -1;
    for (int i=0; i<curplacement.size(); i++) {
        if (curplacement[i] == fail_node_id)
            fail_block_idx = i;
    }

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();

    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    //LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1

    int realLeaves = 0;
    vector<int> avoid_node_ids;
    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;

        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }
    //LOG << "realLeaves: " << realLeaves << endl;

    // 2. color header
    int repair_node_id = -1;
    vector<int> candidates;
    if (scenario == "standby")
        repair_node_id = fail_node_id;
    else{
        // choose a random node apart from the batch placement node
        for (int i=0; i<num_agents; i++) {
            if (find(placement.begin(), placement.end(), i) == placement.end())
            candidates.push_back(i);
        }                        
        if(candidates.size() == 0)
        {
            // choose the minimum inputLoad node as new_node
            int minLoad = INT_MAX;
            for (int i=0; i<num_agents; i++) 
            {
                if(find(avoid_node_ids.begin(), avoid_node_ids.end(), i) != avoid_node_ids.end())
                    continue;
                int inload = loadTable[i][1];
                if(inload < minLoad)
                {
                    minLoad = inload;
                    repair_node_id = i;
                }
            }
        }else {
            int tmpidx = rand() % candidates.size();
            repair_node_id = candidates[tmpidx];
            placement.push_back(repair_node_id);
        }
        LOG << "stripe" << stripe->getStripeId() << " choose the new_node: " << repair_node_id << endl;   
    }
    stripe->_new_node = repair_node_id;
    res.insert(make_pair(ecHeaders[0], repair_node_id));

    // 3. read from the offline solution file for coloring of intermediate nodes
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());

    // note that in offline solution, colors are within ecn
    // if color == fail_block_idx, find corresponding repair node as the real color
    // otherwise, color = block idx, find corresponding node id
    vector<int> itm_offline_coloring = _tp->getColoringByIdx(fail_block_idx);
    //LOG << "itm_idx.size: " << itm_idx.size() << ", itm_offline_coloring.size: " << itm_offline_coloring.size() << endl;

    for (int i=0; i<itm_idx.size(); i++) {
        int dagidx = itm_idx[i];
        int blkidx = itm_offline_coloring[i];

        int nodeid = -1;
        if (blkidx == fail_block_idx)
            nodeid = repair_node_id;
        else
            nodeid = curplacement[blkidx];

        res.insert(make_pair(dagidx, nodeid));
    }

    // add load
    stripe->setColoring(res);
    stripe->evaluateColoring();
    // stripe->dumpTrans(num_agents);
    loadTable = stripe->evalColoringGlobal(loadTable);
}


void ParallelSolution::genColoringForSingleFailure(Stripe* stripe, unordered_map<int, int>& res, 
        int fail_node_id, int num_agents, string scenario, vector<int> & placement) {

    LOG << "ParallelSolution::genColoringForSingleFailure start" << endl;
        
    _num_agents = num_agents;
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();
    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    int fail_block_idx = -1;
    for (int i=0; i<curplacement.size(); i++) {
        if (curplacement[i] == fail_node_id)
            fail_block_idx = i;
    }

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();
    LOG << "number of intermediate vertices: " << intermediate_num << endl;

    // 1. color leave vertices 
    // If a leave vertex is part of a real block, we first figure out its block index, and then find corresponding node id
    // If a leave vertex is part of a virtual block (shortening), we mark nodeid as -1
    int realLeaves = 0;
    vector<int> avoid_node_ids;
    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;

        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }
        res.insert(make_pair(dagidx, nodeid));
    }

    // choose new node after coloring
    int repair_node_id;
    vector<int> candidates;
    if (scenario == "standby")
        repair_node_id = fail_node_id;
    else {
        repair_node_id = -1;
        // choose a random node apart from the batch placement node
        for (int i=0; i<num_agents; i++) {
            if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                candidates.push_back(i);
        } 
        LOG << "candidates.size() : "<<candidates.size()<<endl; 

        int tmpidx = rand() % candidates.size();
        repair_node_id = candidates[tmpidx];
        placement.push_back(repair_node_id);
    }
    res.insert(make_pair(ecHeaders[0], repair_node_id));
    // intermediate node idxs
    vector<int> itm_idx;
    for (auto item: ecNodeMap) {
        int dagidx = item.first;
        if (find(ecHeaders.begin(), ecHeaders.end(), dagidx) != ecHeaders.end())
            continue;
        if (find(ecLeaves.begin(), ecLeaves.end(), dagidx) != ecLeaves.end())
            continue;
        itm_idx.push_back(dagidx);
    }
    sort(itm_idx.begin(), itm_idx.end());    
    struct timeval time1, time2;
    SingleMLP(stripe ,itm_idx, candidates, ecdag, res);
}


vector<vector<int>> genTable (unordered_map<int,int> out, unordered_map<int,int> in, int _num_agents)
{
    vector<vector<int>> table(_num_agents, {0,0});
    for(int i = 0; i < _num_agents; i++)
    {
        table[i][0] = out[i];
        table[i][1] = in[i];
    }
    return table;
}

void dumpPlacement(vector<RepairBatch*> batch_list)
{
    for(auto batch: batch_list)
    {
        for(auto stripe: batch->getStripeList())
        {
            LOG << " " << stripe->getStripeId();
        }
        LOG << " load=" << batch->getLoad() <<" avgload = " << batch->getLoad()*1.0/ batch->getStripeList().size();
        LOG << endl;
    }
}

void ParallelSolution::improve(int fail_node_id, string scenario){
    // init
    unordered_map<RepairBatch*,int> batch_size_map;
    unordered_map<RepairBatch*, vector<vector<int>>> table_map; 
    unordered_map<int, vector<int>> mem_has_tryed; // batch->tryed_stripe_list: for avoid the repeat computing if the stripe could insert this batch
    int right = _batch_list.size()-1;
    int last_batch_id = _batch_list.size() -1;
    int debug_break;
    
    struct timeval time1, time2;
    gettimeofday(&time1, NULL);
    // 1. color all batchs
    for(int i = 0; i < _batch_list.size(); i++)
    {
        LOG << "[DEBUG] coloring batch: " << i << "/" << _batch_list.size() << endl;
        RepairBatch* batch = _batch_list[i];
        vector<vector<int>> loadTable(_num_agents, {0,0}); // {out,in}
        
        // gen placement
        vector<int> placement;
        for(auto it : batch->getStripeList())
        {
            for(auto idx : it->getPlacement())
            {
                if(find(placement.begin(), placement.end(), idx) == placement.end())
                {
                    placement.push_back(idx);
                }
            }
        }

        // gen head batch coloring solution
        vector<Stripe*> full_stripe_list = batch->getStripeList();   
        for (int i=0; i<full_stripe_list.size(); i++) {
            Stripe* stripe = full_stripe_list[i];
            ECDAG* ecdag = stripe->genRepairECDAG(_ec, {fail_node_id});
            unordered_map<int, int> coloring;
            genColoringForSingleFailure(stripe, coloring, fail_node_id, _num_agents, scenario, placement);
            //genOfflineColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable, placement);  
            stripe->setColoring(coloring);
            stripe->evaluateColoring();
        }
        vector<vector<int>> table =  batch->getLoadTable(_num_agents);
   

        // record information
        table_map.insert(make_pair(batch,table));
        batch_size_map.insert(make_pair(batch,full_stripe_list.size()));
        batch->evaluateBatch(_num_agents);
    }
    gettimeofday(&time2, NULL);
    double color_all_batch = DistUtil::duration(time1, time2);

    // insert tail batch to pre
    while(true) 
    {   
        gettimeofday(&time1, NULL);
        int debug_try_num = 0;

        // insert one tail batch into pre batch
        sort(_batch_list.begin(), _batch_list.end(), [](RepairBatch* batch1, RepairBatch* batch2){
            return batch1->getLoad()/batch1->getStripeList().size() < batch2->getLoad()/batch2->getStripeList().size();
        });
        bool all_optimized = true;
        vector<pair<Stripe*,RepairBatch*>> log; // for insert entire tail batch into pre batch
        RepairBatch* tail_batch = _batch_list[last_batch_id];
        vector<Stripe*> stripe_list = tail_batch->getStripeList();
        unordered_map<Stripe*, unordered_map<int,int>> coloring_map;
        for(auto tail_stripe : stripe_list)
        {
            // 1. for every batch, try to insert
            int batch_idx = -1;
            coloring_map.insert(make_pair(tail_stripe, tail_stripe->getColoring()));
            for(int i = 0; i < last_batch_id; i++)
            {
                RepairBatch* batch = _batch_list[i];

                // avoid repeat calulate
                auto mem = mem_has_tryed[batch->getBatchId()];
                if(find(mem.begin(), mem.end(), tail_stripe->getStripeId()) != mem.end()){
                    continue;
                }
                mem_has_tryed[batch->getBatchId()].push_back(tail_stripe->getStripeId());
                debug_try_num++;

                // calculate average load before insert tail stripe
                vector<vector<int>> loadTable = table_map[batch];
                int loadpre = evalTable1(loadTable)._load;
                int stripe_num =  batch_size_map[batch];
                float avgpre = loadpre*1.0 / stripe_num;
                 
                // coloring the solution
                unordered_map<int, int> coloring;
                genParallelColoringForSingleFailure(tail_stripe, fail_node_id, _num_agents, scenario, loadTable);                
                // calculate average load after insert tail stripe
                int loadafter = evalTable1(loadTable)._load;
                float avgafter = loadafter*1.0 / (stripe_num + 1);
                
                // LOG << "try insert stripe " << tail_stripe->getStripeId() << " into batch " << batch->getBatchId()    << endl;
                // LOG << "avg load pre = " << loadpre << "/" << stripe_num << " = " << avgpre << endl;
                // LOG << "avg load after = " << loadafter << "/" << stripe_num+1 << " = " << avgafter << endl;

                // matching batch and stripe
                if(avgafter < avgpre){
                    // LOG << "match insert stripe " << tail_stripe->getStripeId() << " into batch " << batch->getBatchId()    << endl;
                    // LOG << "avg load pre = " << loadpre << "/" << stripe_num << " = " << avgpre << endl;
                    // LOG << "avg load after = " << loadafter << "/" << stripe_num+1 << " = " << avgafter << endl;
                    batch_idx = i;
                    batch_size_map[batch]++;
                    table_map[batch] = loadTable;
                    break;
                }             
            }
            
            // 2. this stripe has no prebatch to insert, then the improve progress will stop       
            if(batch_idx == -1) 
            {
                all_optimized = false;
                break;
            }

            // 3. save in log, entirly insert
            log.push_back(make_pair(tail_stripe, _batch_list[batch_idx]));
        }

        if(!all_optimized)
        {
            LOG << "coloring vec size = " << coloring_map.size() << endl;
            LOG << "stripelist_size = " << stripe_list.size() << endl;
            for(auto it : coloring_map)
            {         
                it.first->setColoring(it.second);
                it.first->evaluateColoring();
            }
            break;
        } 
            

        // perform insert based on log
        for(auto pair : log)
        {
            RepairBatch* batch = pair.second;
            Stripe* stripe = pair.first;
            LOG << "perform insert stripe " << stripe->getStripeId() << " into batch " << batch->getBatchId() << endl;
            batch->push(stripe);
            tail_batch->erase(stripe);  
            mem_has_tryed[batch->getBatchId()].clear();    
        }
        _batch_list.erase(_batch_list.begin() + last_batch_id);
        delete tail_batch;
        last_batch_id--;


        gettimeofday(&time2, NULL);
        LOG << "duration " << DistUtil::duration(time1, time2) << " for try matching " << debug_try_num << " times "<< endl;
        LOG << "[DEBUG] " << last_batch_id+1 << "/" << right << endl;
    }
    LOG << "duration first coloring batch = " << color_all_batch << endl;
    LOG << "debug placement2, batch_num = " << _batch_list.size() << endl;
    //dumpPlacement_improve(_batch_list);
}

void ParallelSolution::improve_enqueue(int fail_node_id, string scenario){
    // init
    int debug_break;
    struct timeval time1, time2;
    int min_output_add = 0;
    if(_codename == "Clay")
    {
        // for single clay code
        int q = _ec->_n - _ec->_k;
        int t = _ec->_n / q;
        min_output_add = pow(q,t-1);
    }

    // 1. color all batchs
    gettimeofday(&time1, NULL);
    for(int i = 0; i < _batch_list.size(); i++)
    {
        RepairBatch* batch = _batch_list[i];
        vector<vector<int>> loadTable(_num_agents, {0,0}); // {out,in}
        
        // gen placement
        vector<int> placement;
        for(auto it : batch->getStripeList())
        {
            for(auto idx : it->getPlacement())
            {
                if(find(placement.begin(), placement.end(), idx) == placement.end())
                    placement.push_back(idx);
            }
        }

        // gen head batch coloring solution
        vector<Stripe*> full_stripe_list = batch->getStripeList();   
        for (int i=0; i<full_stripe_list.size(); i++) {
            Stripe* stripe = full_stripe_list[i];
            ECDAG* ecdag = stripe->genRepairECDAG(_ec, {fail_node_id});
            unordered_map<int, int> coloring;
            genColoringForSingleFailure(stripe, coloring, fail_node_id, _num_agents, scenario, placement);
            
            //genOfflineColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable, placement);  
            
        }
        // record information
        batch->evaluateBatch(_num_agents);
    }
    gettimeofday(&time2, NULL);
    double color_all_batch = DistUtil::duration(time1, time2);

    struct timeval improve_start, curr_improve;
    gettimeofday(&improve_start, NULL);
    
    // 2. insert stripe into batch
    while(true)
    {
        // check if has merged all batch
        if(_batch_list.size() == 0)
            break;
        // sort(_batch_list.begin(), _batch_list.end(), [](RepairBatch* batch1, RepairBatch* batch2){
        //     return batch1->getLoad()/batch1->getStripeList().size() < batch2->getLoad()/batch2->getStripeList().size();
        // });
        RepairBatch* head_batch = _batch_list[0];
        int tail_batch_idx = _batch_list.size()-1;
        int stripe_num = head_batch->getStripeList().size();
        float best_avg = evalTable1(head_batch->getLoadTable(_num_agents))._load*1.0/stripe_num;
        
        vector<pair<RepairBatch*, Stripe*>> batch_stripe;
        vector<int> stripeIdx_to_insert;

        // version2
        vector<int> has_tryed;
         
        vector<vector<int>> loadTable = head_batch->getLoadTable(_num_agents); // contain the cache stripe load

        // improve curr batch until the signal turn true
        struct timeval improve1, improve2;
        gettimeofday(&improve1, NULL);

        bool offline = false;
        while(true)
        {
            // insert one stripe into one head_batch
            Stripe* stripe;
            RepairBatch* batch;
            bool find_stripe = false; // find a candidate
            stripe_num = head_batch->getStripeList().size() + batch_stripe.size();

            // select a candidate stripe to insert
            struct timeval newnode1, newnode2;
            gettimeofday(&newnode1, NULL);
            for(int i = tail_batch_idx; i > 0; i--)
            {
                RepairBatch* tail_batch = _batch_list[i];
                for(auto tail_stripe : tail_batch->getStripeList())
                {
                    if(find(stripeIdx_to_insert.begin(), stripeIdx_to_insert.end(), tail_stripe->getStripeId()) != stripeIdx_to_insert.end())
                        continue;
                    // remove source nodes that we should avoid 
                    vector<int> avoid_node_ids = tail_stripe->getPlacement();
                    vector<int> candidates;
                    for (int i=0; i<_num_agents; i++) {
                        if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                            candidates.push_back(i);
                    }

                    // choose the minimum inputload apart from source node 
                    int min_load = INT_MAX;
                    int min_load_color = -1;
                    for(auto it : candidates)
                    {
                        int inload = loadTable[it][1];
                        if(inload < min_load)
                        {
                            min_load = inload;
                            min_load_color = it;
                        }
                    }
                    // aovoid the new_node input_load produce new max input_load
                    int new_node_load = loadTable[min_load_color][1] + _ec->_w;
                    if(new_node_load*1.0/(stripe_num+1) > best_avg){
                        if(DEBUG_ENABLE)
                            LOG << "stripe" << tail_stripe->getStripeId() <<  " new_node input load is to large, = " << new_node_load << " avg=" << new_node_load*1.0/(stripe_num+1) << endl;
                        continue;
                    }
                    
                    // choose the maximum outputload amoung stripe source_node
                    int max_load = 0;
                    int max_load_color = -1;
                    vector<int> source_node_ids = tail_stripe->getPlacement();
                    source_node_ids.erase(find(source_node_ids.begin(), source_node_ids.end(), fail_node_id));
                    for(auto it : source_node_ids)
                    {
                        int outLoad = loadTable[it][0];
                        if(outLoad > max_load)
                        {
                            max_load = outLoad;
                            max_load_color = it;
                        }
                    }
                    int max_gloabl_output = max_load+min_output_add;
                    double avg_output = max_gloabl_output*1.0/(stripe_num+1);
                    if(DEBUG_ENABLE)
                        LOG << "output avg = " << max_gloabl_output << "/" << (stripe_num+1) << "=" << avg_output << endl;
                    if(avg_output > best_avg){
                        if(DEBUG_ENABLE)
                            LOG << "soucenode output load is to large" << endl;
                        continue;
                    }

                    find_stripe = true;
                    stripe = tail_stripe;
                    break;
                }
                if(find_stripe){
                    batch = tail_batch;
                    break;
                }
            }
            if(!find_stripe){ // search all stripe
                LOG << "search all stripe, curr batch size = " << head_batch->getStripeList().size() << endl;
                break;
            }
            gettimeofday(&newnode2, NULL);

            // coloring the solution
            has_tryed.push_back(stripe->getStripeId());
            gettimeofday(&time1, NULL);
            batch_stripe.push_back(make_pair(batch,stripe));
            genParallelColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable);

            // calculate average load after insert tail stripe
            int loadafter = evalTable1(loadTable)._load;
            int bdwtafter = evalTable1(loadTable)._bdwt;
            float avgafter = loadafter*1.0 / (stripe_num + 1);
            gettimeofday(&time2, NULL);

            // dump
            LOG << "insert stripe " << stripe->getStripeId() << " into batch " << head_batch->getBatchId() 
                << " coloring duration = " << DistUtil::duration(time1, time2) << endl;
            LOG << "best avg load = " << evalTable1(head_batch->getLoadTable(_num_agents +_standby_size))._load << "/" << head_batch->getStripeList().size() << " = " << best_avg << endl;
            LOG << "batch.load = " << head_batch->getLoad() << endl;
            LOG << "avg load after = " << loadafter << "/" << stripe_num+1 << " = " << avgafter << endl;
            gettimeofday(&curr_improve, NULL);
            double curr_time = DistUtil::duration(improve_start, curr_improve);
            LOG << curr_time/1000 << " " << stripe_num  << " " << loadafter/_ec->_w  << " " << bdwtafter/_ec->_w << " " <<  avgafter << endl; 
            if(avgafter > best_avg){
                // cache
                stripeIdx_to_insert.push_back(stripe->getStripeId());
            }else{
                // perform cache merge
                for(auto it : batch_stripe)
                {
                    RepairBatch* batch_to_delete = it.first;
                    Stripe* stripe_to_insert = it.second;
                    head_batch->push(stripe_to_insert);
                    batch_to_delete->erase(stripe_to_insert);
                    if(batch_to_delete->getStripeList().size() == 0)
                    {
                        _batch_list.erase(find(_batch_list.begin(), _batch_list.end(), batch_to_delete));
                    }
                }
                batch_stripe.clear();
                stripeIdx_to_insert.clear();
                best_avg = avgafter;
            }
        }

    
        LOG << "curr batch finish" << endl;
        head_batch->dump();
        
        _batch_queue.push(head_batch);
        _batch_list.erase(_batch_list.begin());

        _lock.lock();
        _batch_request = false;
        _lock.unlock();

        gettimeofday(&improve2, NULL);


        // dump
        LOG << "Improve batch" << head_batch->getBatchId() << " for " << DistUtil::duration(improve1, improve2) << endl;
        int avgload = head_batch->getLoad() / head_batch->getStripeList().size();
        LOG << "avg load = " << head_batch->getLoad() << "/" << head_batch->getStripeList().size()  << " = " << avgload << endl;
    }
    return;
}

void ParallelSolution::improve_multiple(vector<int> fail_node_ids, string scenario ,int method ){
    // init
    if(DEBUG_ENABLE)
        LOG  << "ParallelSolution::improve_multiple start" << endl;
    int debug_break;
    struct timeval time1, time2;
    struct timeval time1_color, time2_color;
    int min_output_add = 0;
    if(_codename == "Clay")
    {
        // for single clay code
        int q = _ec->_n - _ec->_k;
        int t = _ec->_n / q;
        min_output_add = pow(q,t-1);
    }

    // 1. color all batchs
    gettimeofday(&time1, NULL);
    for(int i = 0; i < _batch_list.size(); i++)
    {
        RepairBatch* batch = _batch_list[i];
        vector<vector<int>> loadTable(_num_agents, {0,0}); // {out,in}
        
        // gen placement
        vector<int> placement;
        for(auto it : batch->getStripeList())
        {
            for(auto idx : it->getPlacement())
            {
                if(find(placement.begin(), placement.end(), idx) == placement.end())
                    placement.push_back(idx);
            }
        }

        // gen head batch coloring solution
        vector<Stripe*> full_stripe_list = batch->getStripeList();   
        for (int i=0; i<full_stripe_list.size(); i++) {
            Stripe* stripe = full_stripe_list[i];
            ECDAG* ecdag = stripe->genRepairECDAG(_ec, fail_node_ids);
            unordered_map<int, int> coloring; 
            //genColoringForMultipleFailure(stripe, coloring, fail_node_ids, _num_agents, scenario, placement,0);
            LOG<<"in improve_multiple agent_num"<<_num_agents<<endl;
            
            //cout<<"in improve_multiple  _conf->_blkDir:"<<_conf->_blkDir<<endl;
            gettimeofday(&time1_color, NULL);
            // if(stripe->getFailNum()==1){
            //     genColoringForSingleFailure(stripe, coloring, fail_node_ids[0], _num_agents, scenario, placement);
            // }else{
                genColoringForMultipleFailureLevel(stripe, coloring, fail_node_ids, _num_agents, scenario, placement,method);
//            }
            gettimeofday(&time2_color, NULL);

            double t_color = DistUtil::duration(time1_color, time2_color);

            cout<<"!!!!!!!!!!2270 hang t_color:"<<t_color<<endl;
            
            stripe->dumpLoad(_num_agents + _standby_size);
        }
        // record information
        batch->evaluateBatch( _num_agents + _standby_size );   
    }
    
    LOG << "2457 hang ParallelSolution::improve_multiple"<<endl;
    for(auto it : _batch_list){
        it->dump(_num_agents + _standby_size);
    }
    gettimeofday(&time2, NULL);
    double color_all_batch = DistUtil::duration(time1, time2);

    struct timeval improve_start, curr_improve;
    gettimeofday(&improve_start, NULL);
    
    // 2. insert stripe into batch
    while(true)
    {
        // check if has merged all batch
        if(_batch_list.size() == 0)
            break;

        sort(_batch_list.begin(), _batch_list.end(), [](RepairBatch* batch1, RepairBatch* batch2){
            return batch1->getLoad()/batch1->getStripeList().size() < batch2->getLoad()/batch2->getStripeList().size();
        });

        // FIXME: test for Hungary sort, need to delete after test
        // sort(_batch_list.begin(), _batch_list.end(), [](RepairBatch* batch1, RepairBatch* batch2){
        //     if(batch1->getStripeList().size() != batch2->getStripeList().size()){
        //         return batch1->getStripeList().size() > batch2->getStripeList().size();
        //     }
        //     return batch1->getBdwt() > batch2->getBdwt();
        // });
        int failure_1 = 0;
        int failure_2 = 0;
        int curr_size = _batch_list[0]->getStripeList().size();
        LOG << "2350 hang debug hungary" << endl;
        for(auto batch: _batch_list)
        {
            
            auto vec = batch->getStripeList();
            int size = vec.size();
            if(size != curr_size){
                LOG << "["<< failure_1 << " " << failure_2 << "]" << endl;
                curr_size = size;
                failure_1 = 0;
                failure_2 = 0;
            }
            LOG << "batch(" << batch->getBatchId()<< ")  ";
            sort(vec.begin(), vec.end(), [](Stripe* a, Stripe* b){
                return a->getStripeId() < b->getStripeId();
            });
            for(auto stripe: vec)
            {
                LOG << " " << stripe->getStripeId();
                if(stripe->getStripeId() < 200){
                    failure_1++;
                }else{
                    failure_2++;
                }
            }
            LOG << " : " << batch->getLoad() << "  " << batch->getBdwt() << endl; 
        }
        LOG << "["<< failure_1 << " " << failure_2 << "]" << endl;
        // exit(1);
        
        RepairBatch* head_batch = _batch_list[0];
        int tail_batch_idx = _batch_list.size()-1;
        int stripe_num = head_batch->getStripeList().size();
        float best_avg = evalTable1(head_batch->getLoadTable(_num_agents + _standby_size))._load*1.0/stripe_num;
        
        vector<pair<RepairBatch*, Stripe*>> batch_stripe;
        vector<int> stripeIdx_to_insert;
        vector<vector<int>> loadTable = head_batch->getLoadTable(_num_agents+ _standby_size); // contain the cache stripe load

        // improve curr batch until the signal turn true
        struct timeval improve1, improve2;
        gettimeofday(&improve1, NULL);
        while(true)
        {
            // insert one stripe into one head_batch
            Stripe* stripe;
            RepairBatch* batch;
            bool find_stripe = false; // find a candidate
            stripe_num = head_batch->getStripeList().size() + batch_stripe.size();

            // select a candidate stripe to insert
            struct timeval newnode1, newnode2;
            gettimeofday(&newnode1, NULL);
            for(int i = tail_batch_idx; i > 0; i--)
            {   
                RepairBatch* tail_batch = _batch_list[i];
                for(auto tail_stripe : tail_batch->getStripeList())
                {
                    if(find(stripeIdx_to_insert.begin(), stripeIdx_to_insert.end(), tail_stripe->getStripeId()) != stripeIdx_to_insert.end())
                        continue;

                    // remove source nodes that we should avoid 
                    vector<int> avoid_node_ids = tail_stripe->getPlacement();
                    vector<int> candidates;
                    for (int i=0; i<_num_agents; i++) {
                        if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                            candidates.push_back(i);
                    }

                    // choose the minimum inputload apart from source node 
                    int min_load = INT_MAX;
                    int min_load_color = -1;
                    for(auto it : candidates)
                    {
                        int inload = loadTable[it][1];
                        if(inload < min_load)
                        {
                            min_load = inload;
                            min_load_color = it;
                        }
                    }
                    
                    // aovoid the new_node input_load produce new max input_load
                    int new_node_load = loadTable[min_load_color][1] + _ec->_w;
                    if(new_node_load*1.0/(stripe_num+1) > best_avg){
                        if(DEBUG_ENABLE)
                            LOG << "stripe" << tail_stripe->getStripeId() <<  " new_node input load is to large, = " << new_node_load << " avg=" << new_node_load*1.0/(stripe_num+1) << endl;
                        continue;
                    }
                    
                    // choose the maximum outputload amoung stripe source_node
                    int max_load = 0;
                    int max_load_color = -1;
                    vector<int> source_node_ids = tail_stripe->getPlacement();

                    for(auto failnodeid: fail_node_ids){
                        auto it = find(source_node_ids.begin(), source_node_ids.end(), failnodeid);
                        if(it != source_node_ids.end()){
                            source_node_ids.erase(it);
                        }          
                    }   

                    for(auto it : source_node_ids)
                    {
                        int outLoad = loadTable[it][0];
                        if(outLoad > max_load)
                        {
                            max_load = outLoad;
                            max_load_color = it;
                        }
                    }
 
                    int max_gloabl_output = max_load+min_output_add;
                    double avg_output = max_gloabl_output*1.0/(stripe_num+1);
                    if(DEBUG_ENABLE)
                        LOG << "output avg = " << max_gloabl_output << "/" << (stripe_num+1) << "=" << avg_output << endl;
                    if(avg_output > best_avg){
                        if(DEBUG_ENABLE)
                            LOG << "soucenode output load is to large" << endl;
                        continue;
                    }
                    find_stripe = true;
                    stripe = tail_stripe;
                    break;
                }
                if(find_stripe){
                    batch = tail_batch;
                    break;
                }
            }
            if(!find_stripe){ // search all stripe
                LOG << "search all stripe, curr batch size = " << head_batch->getStripeList().size() << endl;
                break;
            }else{
                LOG << "choose stripe " << stripe->getStripeId() << endl;
            }
            gettimeofday(&newnode2, NULL);

            // coloring the solution
            gettimeofday(&time1, NULL);
            batch_stripe.push_back(make_pair(batch,stripe));
            // LOG << "before coloring" << endl;
            // dumpTable(loadTable);
            genParallelColoringForMultipleFailure(stripe, fail_node_ids, _num_agents, scenario, loadTable);
            //stripe->dumpLoad(_num_agents);
            //LOG << "after coloring " << endl;
            //dumpTable(loadTable);

            // calculate average load after insert tail stripe
            int loadafter = evalTable1(loadTable)._load;
            int bdwtafter = evalTable1(loadTable)._bdwt;
            float avgafter = loadafter*1.0 / (stripe_num + 1);
            gettimeofday(&time2, NULL);

            // dump
            LOG << "insert stripe " << stripe->getStripeId() << " into batch " << head_batch->getBatchId() 
                << " coloring duration = " << DistUtil::duration(time1, time2) << endl;
            LOG << "best avg load = " << evalTable1(head_batch->getLoadTable(_num_agents + _standby_size))._load << "/" << head_batch->getStripeList().size() << " = " << best_avg << endl;
            LOG << "batch.load = " << head_batch->getLoad() << endl;
            LOG << "avg load after = " << loadafter << "/" << stripe_num+1 << " = " << avgafter << endl;
            gettimeofday(&curr_improve, NULL);
            double curr_time = DistUtil::duration(improve_start, curr_improve);
            LOG << curr_time/1000 << " " << stripe_num  << " " << loadafter/_ec->_w  << " " << bdwtafter/_ec->_w << " " <<  avgafter << endl; 
            if(avgafter > best_avg){
                // cache
                stripeIdx_to_insert.push_back(stripe->getStripeId());
            }else{
                // perform cache merge
                for(auto it : batch_stripe)
                {
                    RepairBatch* batch_to_delete = it.first;
                    Stripe* stripe_to_insert = it.second;
                    head_batch->push(stripe_to_insert);
                    batch_to_delete->erase(stripe_to_insert);
                    if(batch_to_delete->getStripeList().size() == 0)
                    {
                        _batch_list.erase(find(_batch_list.begin(), _batch_list.end(), batch_to_delete));
                    }
                }
                batch_stripe.clear();
                stripeIdx_to_insert.clear();
                best_avg = avgafter;
            }
        }

        LOG << "Curr batch finish" << endl;
        head_batch->dump(_num_agents + _standby_size);
        
        _batch_queue.push(head_batch);
        _batch_list.erase(_batch_list.begin());

        _lock.lock();
        _batch_request = false;
        _lock.unlock();

        gettimeofday(&improve2, NULL);


        // dump
        LOG << "Improve batch" << head_batch->getBatchId() << " for " << DistUtil::duration(improve1, improve2) << endl;
        int avgload = head_batch->getLoad() / head_batch->getStripeList().size();
        LOG << "avg load = " << head_batch->getLoad() << "/" << head_batch->getStripeList().size()  << " = " << avgload << endl;
    }
    return;
}

void ParallelSolution::improve_hybrid(int fail_node_id, string scenario){
    if(DEBUG_ENABLE)
        LOG << "ParallelSolution::improve_hybrid" << endl;
    // init
    int debug_break;
    struct timeval time1, time2, time3, time4, time5, time6;
    
    // 1. color all batchs
    gettimeofday(&time1, NULL);
    for(int i = 0; i < _batch_list.size(); i++)
    {
        // LOG << "[DEBUG] coloring batch: " << i << "/" << _batch_list.size() << endl;
        RepairBatch* batch = _batch_list[i];
        vector<vector<int>> loadTable(_num_agents, {0,0}); // {out,in}
        
        // gen placement
        vector<int> placement;
        for(auto it : batch->getStripeList())
        {
            for(auto idx : it->getPlacement())
            {
                if(find(placement.begin(), placement.end(), idx) == placement.end())
                    placement.push_back(idx);
            }
        }

        // gen head batch coloring solution
        vector<Stripe*> full_stripe_list = batch->getStripeList();   
        for (int i=0; i<full_stripe_list.size(); i++) {
            Stripe* stripe = full_stripe_list[i];
            ECDAG* ecdag = stripe->genRepairECDAG(_ec, {fail_node_id});
            // genColoringForSingleFailure(stripe, coloring, fail_node_id, _num_agents, scenario, placement);
            genOfflineColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable, placement);  
        }

        // record information
        batch->evaluateBatch(_num_agents);
    }
    gettimeofday(&time2, NULL);
    double color_all_batch = DistUtil::duration(time1, time2);

    // 2. insert stripe into batch
    while(true)
    {
        // check if has merged all batch
        if(_batch_list.size() == 0)
            break;
        sort(_batch_list.begin(), _batch_list.end(), [](RepairBatch* batch1, RepairBatch* batch2){
            return batch1->getLoad()/batch1->getStripeList().size() < batch2->getLoad()/batch2->getStripeList().size();
        });

        RepairBatch* head_batch = _batch_list[0];
        if(DEBUG_ENABLE)
            LOG << endl << "imporove for batch" << head_batch->getBatchId() << endl;
        int tail_batch_idx = _batch_list.size()-1;
        int stripe_num = head_batch->getStripeList().size();
        float best_avg = evalTable1(head_batch->getLoadTable(_num_agents + _standby_size))._load*1.0/stripe_num;
        
        vector<pair<RepairBatch*, Stripe*>> batch_stripe;
        vector<int> stripeIdx_to_insert;
        vector<vector<int>> loadTable = head_batch->getLoadTable(_num_agents + _standby_size); // contain the cache stripe load

        // improve curr batch until the signal turn true
        struct timeval improve1, improve2;
        gettimeofday(&improve1, NULL);

        bool offline = false;
        while(true)
        {
            if(_batch_request && batch_stripe.size() + head_batch->getStripeList().size() >= _batch_size / 2){
                break;
            }
            // insert one stripe into one head_batch
            Stripe* stripe;
            RepairBatch* batch;
            bool find_stripe = false; // find a candidate
            
            // 1. select a candidate stripe to insert
            int debug_pass_input = 0, debug_pass_output = 0;
            gettimeofday(&time3, NULL);
            for(int i = tail_batch_idx; i > 0; i--)
            {
                RepairBatch* tail_batch = _batch_list[i];
                for(auto tail_stripe : tail_batch->getStripeList())
                {
                    if(find(stripeIdx_to_insert.begin(), stripeIdx_to_insert.end(), tail_stripe->getStripeId()) != stripeIdx_to_insert.end())
                        continue;

                    // remove source nodes that we should avoid 
                    vector<int> avoid_node_ids = tail_stripe->getPlacement();
                    LOG << "source node" << endl;
                    for(auto it : avoid_node_ids)
                    {
                        LOG << " " << it;
                    }
                    LOG << endl;
                    dumpTable(loadTable);
                    vector<int> candidates;
                    for (int i=0; i<_num_agents; i++) {
                        if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                            candidates.push_back(i);
                    }

                    // choose the minimum inputload apart from source node 
                    int min_load = INT_MAX;
                    int min_load_color = -1;
                    for(auto it : candidates)
                    {
                        int inload = loadTable[it][1];
                        if(inload < min_load)
                        {
                            min_load = inload;
                            min_load_color = it;
                        }
                    }

                    // aovoid the new_node input_load produce new max input_load
                    int new_node_load = loadTable[min_load_color][1] + _ec->_w;
                    if(new_node_load*1.0/(stripe_num+1) > best_avg){
                        if(DEBUG_ENABLE)
                            LOG << "new_node input load is to large pass" << endl;
                        debug_pass_input++;
                        continue;
                    }
                    
                    // choose the maximum outputload amoung stripe source_node
                    int max_output = 0;
                    int max_output_color = -1;
                    vector<vector<int>> tempTable = deepCopyTable(loadTable);
                    for(auto leaf : tail_stripe->getECDAG()->getECLeaves()){
                        int blkid = leaf/_ec->_w;
                        int nodeid = tail_stripe->getPlacement()[blkid];
                        if(blkid < _ec->_n){
                            tempTable[nodeid][0]++;
                        }
                    }
                    LOG << "debug output load" << endl;
                    dumpTable(tempTable);
                    
                    for(int i = 0; i < tempTable.size(); i++)
                    {
                        int outLoad = tempTable[i][0];
                        if(outLoad > max_output)
                        {
                            max_output = outLoad;
                            max_output_color = i;
                        }
                    }
                    float avg_output = max_output*1.0/(stripe_num+1);
                    if(avg_output > best_avg){
                        LOG << "source node output load is to large" << max_output << "/" << stripe_num+1 << "=" << avg_output << ", pass" << endl;
                        debug_pass_output++;
                        continue;
                    }
                    find_stripe = true;
                    stripe = tail_stripe;
                    break;
                }
                if(find_stripe){
                    batch = tail_batch;
                    break;
                }
            }
            if(!find_stripe){ // search all stripe
                if(DEBUG_ENABLE)
                    LOG << "search all stripe, curr batch size = " << head_batch->getStripeList().size() << endl;
                break;
            }
            gettimeofday(&time4, NULL);
            double select_candidate = DistUtil::duration(time3, time4);
            // 2. try coloring the solution
            gettimeofday(&time5, NULL);
            batch_stripe.push_back(make_pair(batch,stripe));

            vector<int> placement;

            if(batch_stripe.size() + head_batch->getStripeList().size() < _batch_size / 2){
                genOfflineColoringForSingleFailure(stripe,fail_node_id, _num_agents, scenario, loadTable, placement);
            }else{
                genParallelColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable);
            }
            
            // calculate average load after insert tail stripe
            int loadafter = evalTable1(loadTable)._load;
            float avgafter = loadafter*1.0 / (stripe_num + 1);
            gettimeofday(&time6, NULL);
            double try_merge = DistUtil::duration(time5,time6);

            if(DEBUG_ENABLE)
            {
                LOG << "select candidate duration: " << select_candidate 
                    << " , and pass intput:" << debug_pass_input << ", pass output:" << debug_pass_output << endl;
                LOG << "insert stripe " << stripe->getStripeId() << " into batch " << head_batch->getBatchId() 
                    << "  duration: " << try_merge << endl;
                LOG << "best avg load = " << evalTable1(head_batch->getLoadTable(_num_agents + _standby_size))._load << "/" << head_batch->getStripeList().size() << " = " << best_avg << endl;
                LOG << "batch.load = " << head_batch->getLoad() << endl;
                LOG << "avg load after = " << loadafter << "/" << stripe_num+1 << " = " << avgafter << endl;
            }
            // 3. cache or perform merge
            stripe_num++; 
            if(avgafter > best_avg){
                // cache
                stripeIdx_to_insert.push_back(stripe->getStripeId());
            }else{
                // perform merge
                if(DEBUG_ENABLE)
                    LOG << "perform insert " << batch_stripe.size() << " stripes" << endl;
                for(auto it : batch_stripe)
                {
                    RepairBatch* batch_to_delete = it.first;
                    Stripe* stripe_to_insert = it.second;
                    head_batch->push(stripe_to_insert);
                    batch_to_delete->erase(stripe_to_insert);
                    if(batch_to_delete->getStripeList().size() == 0)
                    {
                        _batch_list.erase(find(_batch_list.begin(), _batch_list.end(), batch_to_delete));
                    }
                }
                batch_stripe.clear();
                stripeIdx_to_insert.clear();
                best_avg = avgafter;
            }
        }
        gettimeofday(&improve2, NULL);

        _batch_queue.push(head_batch);
        _batch_list.erase(_batch_list.begin());

        _lock.lock();
        _batch_request = false;
        _lock.unlock();


        // dump
        if(DEBUG_ENABLE){
            LOG << "Improve batch" << head_batch->getBatchId() << " for " << DistUtil::duration(improve1, improve2) << endl;
            float avgload = head_batch->getLoad() * 1.0 / head_batch->getStripeList().size();
            LOG << "avg load = " << head_batch->getLoad() << "/" << head_batch->getStripeList().size()  << " = " << avgload << endl;
        }
    }
    return;
}

void ParallelSolution::improve_hungary(int fail_node_id, string scenario){
    // init
    int debug_break;
    struct timeval time1, time2;

    bool signal = 0;

    int min_output_add = 0;
    if(_codename == "Clay")
    {
        // for single clay code
        int q = _ec->_n - _ec->_k;
        int t = _ec->_n / q;
        min_output_add = pow(q,t-1);
    }
    // LOG << "debug gamma = " << min_output_add;

    // 1. color all batchs
    gettimeofday(&time1, NULL);
    for(int i = 0; i < _batch_list.size(); i++)
    {
        // LOG << "[DEBUG] coloring batch: " << i << "/" << _batch_list.size() << endl;
        RepairBatch* batch = _batch_list[i];
        vector<vector<int>> loadTable(_num_agents, {0,0}); // {out,in}
        
        // gen placement
        vector<int> placement;
        for(auto it : batch->getStripeList())
        {
            for(auto idx : it->getPlacement())
            {
                if(find(placement.begin(), placement.end(), idx) == placement.end())
                    placement.push_back(idx);
            }
        }

        // gen head batch coloring solution
        vector<Stripe*> full_stripe_list = batch->getStripeList();   
        for (int i=0; i<full_stripe_list.size(); i++) {
            Stripe* stripe = full_stripe_list[i];
            ECDAG* ecdag = stripe->genRepairECDAG(_ec, {fail_node_id});
            // genColoringForSingleFailure(stripe, coloring, fail_node_id, _num_agents, scenario, placement);
            genOfflineColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable, placement);  
        }
        // record information
        batch->evaluateBatch(_num_agents);
    }
    gettimeofday(&time2, NULL);
    double color_all_batch = DistUtil::duration(time1, time2);

    // 2. insert stripe into batch
    while(true)
    {
        // check if has merged all batch
        if(_batch_list.size() == 0)
            break;
        // sort(_batch_list.begin(), _batch_list.end(), [](RepairBatch* batch1, RepairBatch* batch2){
        //     return batch1->getLoad()/batch1->getStripeList().size() < batch2->getLoad()/batch2->getStripeList().size();
        // });
        RepairBatch* head_batch = _batch_list[0];
        int tail_batch_idx = _batch_list.size()-1;
        int stripe_num = head_batch->getStripeList().size();
        float best_avg = evalTable1(head_batch->getLoadTable(_num_agents + _standby_size))._load*1.0/stripe_num;
        
        vector<pair<RepairBatch*, Stripe*>> batch_stripe;
        vector<int> stripeIdx_to_insert;
        vector<vector<int>> loadTable = head_batch->getLoadTable(_num_agents + _standby_size); // contain the cache stripe load

        // improve curr batch until the signal turn true
        struct timeval improve1, improve2;
        gettimeofday(&improve1, NULL);

        // while(_batch_request == false)
        // {
        //     // insert one stripe into one head_batch
        //     Stripe* stripe;
        //     RepairBatch* batch;
        //     bool find_stripe = false; // find a candidate
        //     // select a candidate stripe to insert
        //     struct timeval newnode1, newnode2;
        //     gettimeofday(&newnode1, NULL);
        //     for(int i = tail_batch_idx; i > 0; i--)
        //     {
        //         RepairBatch* tail_batch = _batch_list[i];
        //         for(auto tail_stripe : tail_batch->getStripeList())
        //         {
        //             if(find(stripeIdx_to_insert.begin(), stripeIdx_to_insert.end(), tail_stripe->getStripeId()) != stripeIdx_to_insert.end())
        //                 continue;
        //             // remove source nodes that we should avoid 
        //             vector<int> avoid_node_ids = tail_stripe->getPlacement();
        //             vector<int> candidates;
        //             for (int i=0; i<_num_agents; i++) {
        //                 if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
        //                     candidates.push_back(i);
        //             }
        //             // choose the minimum inputload apart from source node 
        //             int min_load = INT_MAX;
        //             int min_load_color = -1;
        //             for(auto it : candidates)
        //             {
        //                 int inload = loadTable[it][1];
        //                 if(inload < min_load)
        //                 {
        //                     min_load = inload;
        //                     min_load_color = it;
        //                 }
        //             }
        //             // aovoid the new_node input_load produce new max input_load
        //             int new_node_load = loadTable[min_load_color][1] + _ec->_w;
        //             if(new_node_load*1.0/(stripe_num+1) > best_avg){
        //                 LOG << "new_node input load is to large, = " << new_node_load << " avg=" << new_node_load*1.0/(stripe_num+1) << endl;
        //                 continue;
        //             }      
        //             // choose the maximum outputload amoung stripe source_node
        //             int max_load = 0;
        //             int max_load_color = -1;
        //             vector<int> source_node_ids = tail_stripe->getPlacement();
        //             source_node_ids.erase(find(source_node_ids.begin(), source_node_ids.end(), fail_node_id));
        //             for(auto it : source_node_ids)
        //             {
        //                 int outLoad = loadTable[it][0];
        //                 if(outLoad > max_load)
        //                 {
        //                     max_load = outLoad;
        //                     max_load_color = it;
        //                 }
        //             }
        //             int max_gloabl_output = max_load+min_output_add;
        //             int avg_output = max_gloabl_output*1.0/(stripe_num+1);
        //             // LOG << "output avg = " << max_gloabl_output << "/" << (stripe_num+1) << "=" << avg_output << endl;
        //             if(avg_output > best_avg){
        //                 // LOG << "soucenode output load is to large" << endl;
        //                 continue;
        //             }
        //             find_stripe = true;
        //             stripe = tail_stripe;
        //             break;
        //         }
        //         if(find_stripe){
        //             batch = tail_batch;
        //             break;
        //         }
        //     }
        //     if(!find_stripe){ // search all stripe
        //         break;
        //     }
        //     gettimeofday(&newnode2, NULL);
        //     // coloring the solution
        //     gettimeofday(&time1, NULL);
        //     batch_stripe.push_back(make_pair(batch,stripe));
        //     genParallelColoringForSingleFailure(stripe, fail_node_id, _num_agents, scenario, loadTable);
        //     // calculate average load after insert tail stripe
        //     int loadafter = evalTable(loadTable)._load;
        //     float avgafter = loadafter*1.0 / (stripe_num + 1);
        //     gettimeofday(&time2, NULL);
        //     // dump
        //     // LOG << "insert stripe " << stripe->getStripeId() << " into batch " << head_batch->getBatchId() 
        //     //     << " coloring duration = " << DistUtil::duration(time1, time2) << endl;
        //     // LOG << "best avg load = " << evalTable(head_batch->getLoadTable(_num_agents))._load << "/" << head_batch->getStripeList().size() << " = " << best_avg << endl;
        //     // LOG << "batch.load = " << head_batch->getLoad() << endl;
        //     // LOG << "avg load after = " << loadafter << "/" << stripe_num+1 << " = " << avgafter << endl;
        //     stripe_num++; 
        //     if(avgafter > best_avg){
        //         // cache
        //         stripeIdx_to_insert.push_back(stripe->getStripeId());
        //     }else{
        //         // perform merge
        //         for(auto it : batch_stripe)
        //         {
        //             RepairBatch* batch_to_delete = it.first;
        //             Stripe* stripe_to_insert = it.second;
        //             head_batch->push(stripe_to_insert);
        //             batch_to_delete->erase(stripe_to_insert);
        //             if(batch_to_delete->getStripeList().size() == 0)
        //             {
        //                 _batch_list.erase(find(_batch_list.begin(), _batch_list.end(), batch_to_delete));
        //             }
        //         }
        //         batch_stripe.clear();
        //         stripeIdx_to_insert.clear();
        //         best_avg = avgafter;
        //     }
        // }

        _batch_queue.push(head_batch);
        _batch_list.erase(_batch_list.begin());

        _lock.lock();
        _batch_request = false;
        _lock.unlock();

        gettimeofday(&improve2, NULL);


        // dump
        LOG << "Improve batch" << head_batch->getBatchId() << " for " << DistUtil::duration(improve1, improve2) << endl;
        int avgload = head_batch->getLoad() / head_batch->getStripeList().size();
        LOG << "avg load = " << head_batch->getLoad() << "/" << head_batch->getStripeList().size()  << " = " << avgload << endl;
    }
    return;
}

State ParallelSolution::evalTable1(const vector<vector<int>> & table)
{
    int bdwt = 0;
    int load = 0;
    for(auto item: table)
    {
        // bdwt += item[0]; // out
        bdwt += item[1]; // in
        load = max(load, item[0]); 
        load = max(load, item[1]);
    }
    return State(load,bdwt);
}

State ParallelSolution::evalTable(vector<vector<int>> table, vector<int> colors)
{
    // return the load which is the maximum load among the colors_node
    // return the bdwt which is full dag bandwidth
    int bdwt = 0;
    int load = 0;
    for(auto item: table)
    {
        bdwt += item[1]; 
    }

    for(auto color : colors)
    {
        load = max(load, table[color][0]); 
        load = max(load, table[color][1]);
    }
    return State(load,bdwt);
}



vector<int> genBitVec(vector<int> locList, int clusterSize)
{
    vector<int> bitVec(clusterSize, 0);
    for(auto it : locList)
    {
        bitVec[it] = 1;
    }
    return bitVec;
}

Stripe* ParallelSolution::choose(vector<bool> flags, vector<Stripe*> _stripe_list)
{
    LOG << "ParallelSolution::choose begin" << endl;
    Stripe* ret = nullptr;
    vector<int> bitVec(_num_agents, 0);
    for(auto it : _stripe_list){
        for(auto nodeId : it->getPlacement()){
            bitVec[nodeId] = 1;
        }
    }
    auto genBitVec = [&](Stripe* currstripe){
        vector<int> demobitVec(_num_agents,0);
        for(auto nodeId: currstripe->getPlacement()){
            demobitVec[nodeId] = 1;
        }
        return demobitVec;
    };

    auto OR = [&](vector<int> vec_a, vector<int> vec_b){
        int ret = 0;
        for(int i = 0; i < vec_a.size(); i++)
        {
            if(vec_a[i] != 0 || vec_b[i] != 0)
            {
                ret++;
            }
        }
        return ret;
    };
    
    int max_num_cover = OR(bitVec, vector<int>(_num_agents,0));
    for(int i = _batch_list.size()-1; i >= 0; i--)
    {
        RepairBatch* currBatch = _batch_list[i];
        vector<Stripe*> stripe_list = currBatch->getStripeList();
        for(int j = 0; j < stripe_list.size(); j++)
        {
            Stripe* stripe = stripe_list[j];
            if(flags[stripe->getStripeId()])
            {
                // LOG << "has choosen" << endl;
                continue;
            }
                
            vector<int> currBitVec = genBitVec(stripe);
            int orVal = OR(bitVec, currBitVec);
            if(orVal > max_num_cover){
                max_num_cover = orVal;
                ret = stripe;
            }
        }
    }
    if(!ret) return ret;
    // LOG << "choose " << ret->getStripeId() << endl;    
    // LOG << "debug choose" << endl;
    // for(auto it : bitVec)
    // {
    //     // LOG << " " << it;
    // }
    // // LOG << endl;
    auto retBitVec = genBitVec(ret);
    for(auto it : retBitVec){
        // LOG << " " << it;
    }
    // LOG << endl;
    for(int i = 0; i < _num_agents; i++)
    {
        // LOG << " " << bitVec[i] + retBitVec[i];
    }
    // LOG << endl;

    return ret;
}

void ParallelSolution::genRepairBatchesForSingleFailure(int fail_node_id, int num_agents, string scenario) {

    LOG << "ParallelSolution::genRepairBatchesForSingleFailure.fail_node_id = " << fail_node_id << endl;

    // 0. we first figure out stripes that stores a block in $fail_node_id
    filterFailedStripes({fail_node_id});
    vector<Stripe*> stripes_to_repair_vec;
    for(auto idx : _stripes_to_repair){
        stripes_to_repair_vec.push_back(_stripe_list[idx]);
    }
    LOG << "ParallelSolution::genRepairBatchesForSingleFailure.stripes to repair: " << _stripes_to_repair.size() << endl;
   
    // 1. We first perform Hungarian algorithm to divide stripes into batches
    // we also sort batches based on the number of stripes in a batch in descending order
    _num_agents = num_agents;
    struct timeval time1, time2, time3;

    gettimeofday(&time1, NULL);

    // first offline
    // RepairBatch* head_batch = new RepairBatch(0,{});
    // for(int i = 0; i < _batch_size; i++)
    // {
    //     head_batch->push(stripes_to_repair_vec.back());
    //     stripes_to_repair_vec.pop_back();
    //     _stripes_to_repair.pop_back();
    // }  
    // vector<vector<int>> loadTable(_num_agents, {0,0}); // {out,in}
    // vector<int> placement;
    // for(auto it : head_batch->getStripeList())
    // {
    //     for(auto idx : it->getPlacement())
    //     {
    //         if(find(placement.begin(), placement.end(), idx) == placement.end())
    //             placement.push_back(idx);
    //     }
    // }
    // // gen head batch coloring solution
    // vector<Stripe*> head_stripe_list = head_batch->getStripeList();   
    // for (int i=0; i<head_stripe_list.size(); i++) {
    //     Stripe* stripe = head_stripe_list[i];
    //     ECDAG* ecdag = stripe->genRepairECDAG(_ec, fail_node_id);
    //     unordered_map<int, int> coloring;
    //     genOfflineColoringForSingleFailure(stripe, coloring, fail_node_id, _num_agents, scenario, loadTable, placement);  
    // }
    // _batch_queue.push(head_batch);
    // head_batch->evaluateBatch(num_agents);

    _batch_list = findRepairBatchs({fail_node_id}, scenario);
    sort(_batch_list.begin(),_batch_list.end(),[](RepairBatch* batch1, RepairBatch* batch2){
        return batch1->getStripeList().size() > batch2->getStripeList().size();
    });

    // LOG << "3173 hang debug hungary" << endl;
    // for(auto batch: _batch_list)
    // {
    //     LOG << "batch:" << batch->getBatchId();
    //     for(auto stripe: batch->getStripeList())
    //     {
    //         LOG << " " << stripe->getStripeId();
    //     }
    //     LOG << endl;
    // }

    int hungary_num = _batch_list.size();

    // 2. We try to insert stripes of latter batches into previous batches to improve the repair efficiency
    gettimeofday(&time2, NULL);
    if(_enqueue){
        // use blocking queue and signal 
        improve_enqueue(fail_node_id, scenario);
        // improve_hybrid(fail_node_id, scenario);
        // improve_hungary(fail_node_id, scenario);
    }else{
        // generate batch list
        improve(fail_node_id, scenario);
    }
    gettimeofday(&time3, NULL);

    LOG << "[DEBUG] hungary_num = " << hungary_num <<endl;
    LOG << "duration find batch = " << DistUtil::duration(time1,time2)  << endl;
    LOG << "duration merge batch = " << DistUtil::duration(time2,time3) << endl;
}

void ParallelSolution::genRepairBatchesForMultipleFailure(vector<int> fail_node_ids, int num_agents, string scenario,int method) 
{
    //if(DEBUG_ENABLE)
    LOG << "ParallelSolution::genRepairBatchesForMultipleFailure begin" << endl;
    //cout << "[INFO] fail_node_id = " << fail_node_ids << endl;
    // 0. we first figure out stripes that stores a block in $fail_node_id
    filterFailedStripes(fail_node_ids);
    //cout << "[INFO] stripes to repair: " << _stripes_to_repair.size() << endl;
    vector<Stripe*> stripes_to_repair_vec;
    for(auto idx : _stripes_to_repair){
        stripes_to_repair_vec.push_back(_stripe_list[idx]);
    }

    if(DEBUG_ENABLE){
        for(auto stripe : stripes_to_repair_vec){
            LOG << "stripe" << stripe->getStripeId() << ": " ;
            for(auto it : stripe->getPlacement()){
                LOG << " " << it;
            }
            LOG << endl;
        }
        LOG << "ParallelSolution::genRepairBatchesForMultipleFailure.stripes to repair: " << _stripes_to_repair.size() << endl;
    }
    

    // 1. We first perform Hungarian algorithm to divide stripes into batches
    // we also sort batches based on the number of stripes in a batch in descending order
    _num_agents = num_agents;
    struct timeval time1, time2, time3;
    gettimeofday(&time1, NULL);

    _batch_list = findRepairBatchs(fail_node_ids, scenario); // ???
    sort(_batch_list.begin(),_batch_list.end(),[](RepairBatch* batch1, RepairBatch* batch2){
        return batch1->getStripeList().size() > batch2->getStripeList().size();
    });

    LOG << "3375 hang Debug hungary" << endl;
    for(auto batch: _batch_list)
    {
        LOG << "batch" << batch->getBatchId()<<":";
        for(auto stripe: batch->getStripeList())
        {
            LOG << " " << stripe->getStripeId();
        }
        LOG << endl;
    }
    int hungary_num = _batch_list.size();

    // 2. We try to insert stripes of latter batches into previous batches to improve the repair efficiency
    gettimeofday(&time2, NULL);
    if(_enqueue){
        LOG<<"before improve_multiple agent_num"<<_num_agents<<endl;

        improve_multiple(fail_node_ids,scenario,method);

    }else{
        exit(1);
    }
    gettimeofday(&time3, NULL);

    LOG << "[DEBUG] hungary_num = " << hungary_num <<endl; //5 
    LOG << "duration find batch = " << DistUtil::duration(time1,time2)  << endl;
    LOG << "duration merge batch = " << DistUtil::duration(time2,time3) << endl;
}

void ParallelSolution::genRepairBatchesForMultipleFailureNew(vector<int> fail_node_ids, int num_agents, string scenario,int method) 
{
    //if(DEBUG_ENABLE)
    LOG << "ParallelSolution::genRepairBatchesForMultipleFailureNew begin" << endl;
    //cout << "[INFO] fail_node_id = " << fail_node_ids << endl;
    // 0. we first figure out stripes that stores a block in $fail_node_id
    filterFailedStripes(fail_node_ids);
    //cout << "[INFO] stripes to repair: " << _stripes_to_repair.size() << endl;
    vector<Stripe*> stripes_to_repair_vec;
    for(auto idx : _stripes_to_repair){
        stripes_to_repair_vec.push_back(_stripe_list[idx]);
    }

    if(DEBUG_ENABLE){
        for(auto stripe : stripes_to_repair_vec){
            LOG << "stripe" << stripe->getStripeId() << ": " ;
            for(auto it : stripe->getPlacement()){
                LOG << " " << it;
            }
            LOG << endl;
        }
        LOG << "ParallelSolution::genRepairBatchesForMultipleFailureBatch.stripes to repair: " << _stripes_to_repair.size() << endl;
    }
    // 1. we divide stripes to repair into batches of size $batchsize
    _num_batches = _stripes_to_repair.size() / _batch_size;
    if (_stripes_to_repair.size() % _batch_size != 0) {        
        _num_batches += 1; 
    }
    cout << "[INFO] num batches = " << _num_batches << endl;

    for(int batchid = 0 ; batchid < _num_batches;batchid ++){

        cout << "[INFO] INIT BACTH = " << batchid << endl;
        // for one batch

        // 1.initiate
        vector<Stripe*> cur_stripe_list;
        vector<vector<int>> loadtable = vector<vector<int>> (_cluster_size, {0,0});
        // i refers to the i-th stripe in this batch
        for (int i=0; i<_batch_size; i++) {
            // stripeidx refers to the idx in _stripes_to_repair
            int stripeidx = batchid * _batch_size + i;
            if(stripeidx >= _stripes_to_repair.size()) break;
            cout << "[INFO] INIT STRIPE= " << stripeidx << endl;
            // stripeid refers to the actual id of stripe in all the stripes
            int stripeid = _stripes_to_repair[stripeidx];
            Stripe* curstripe = _stripe_list[stripeid];

             // 1.1 construct ECDAG to repair
             ECDAG* curecdag = curstripe->genRepairECDAG(_ec, fail_node_ids);
             curstripe -> refreshECDAG (curecdag);

             // 1.2 generate centralized coloring for the current stripe
            //genParallelColoringForMultipleFailure()
            unordered_map<int, int> coloring; 

            vector<int> nodeidlist = curstripe->getPlacement();

            //genColoringForMultipleFailureLevel(curstripe, coloring, fail_node_ids, num_agents, scenario,nodeidlist ,method);
            //genColoringForMultipleFailureLevelGlobal(curstripe, coloring, fail_node_ids,scenario,loadtable ,method); //zhangJL add
            LOG << "[INFO] INIT STRIPE= " << stripeidx << " fail_num = " << curstripe ->_fail_blk_idxs.size() << endl;
            
            // method = 1; 
            // if (curstripe ->_fail_blk_idxs.size() == 2 || curstripe ->_fail_blk_idxs.size() == 1) {
            //     method = 0 ;
            // }
            if(method == 1){
                int load_hyper = 0 ;
                unordered_map<int, int> coloring_hyper ;
                
                genColoringForMultipleFailureLevelNew(curstripe, fail_node_ids, scenario, loadtable,0);
                load_hyper = curstripe -> _load ;
                coloring_hyper = curstripe -> _coloring ;

                loadtable = vector<vector<int>> (_cluster_size, {0,0});
                int load_multi = 0 ;
                unordered_map<int, int> coloring_multi ;

                genColoringForMultipleFailureLevelNew(curstripe, fail_node_ids, scenario, loadtable,1);
                load_multi = curstripe -> _load ;

                if(load_multi > load_hyper){
                    curstripe -> _coloring = coloring_hyper;
                    curstripe -> setColoring (coloring_hyper);
                    curstripe -> evaluateColoring();
                }
                LOG  << "[INFO] INIT STRIPE= " << stripeidx << endl;
                LOG  << "load_hyper = " << load_hyper << " , "<< "load_multi = " << load_multi << endl ;
            }else{
                genColoringForMultipleFailureLevelNew(curstripe, fail_node_ids, scenario, loadtable,method);
            }

                
            
            
            
            //genColoringForMultipleFailureLevelNN(curstripe,coloring,fail_node_ids,scenario,loadtable,method);

            curstripe->dumpLoad(_cluster_size);
            //curstripe->dumpTrans(_cluster_size);

            // 1.3 insert curstripe into cur_stripe_list
            cur_stripe_list.push_back(curstripe);

            //genBalanceColoringForSingleFailure(curstripe, fail_node_id, scenario, loadtable);
        }
        RepairBatch* curbatch = new RepairBatch(batchid, cur_stripe_list);
        curbatch->evaluateBatch(_cluster_size);
        curbatch->dumpLoad(_cluster_size);

        if (0) {
            _batch_queue.push(curbatch);
        } else {
            _batch_list.push_back(curbatch);
        }
        curbatch->dump();
    }
    
}

void ParallelSolution::prepare(Stripe* stripe, vector<int> fail_node_ids, unordered_map<int, int> & res, string scenario,const vector<vector<int>> & loadTable)
{
    ECDAG* ecdag = stripe->getECDAG();
    vector<int> curplacement = stripe->getPlacement();

    int ecn = _ec->_n;
    int eck = _ec->_k;
    int ecw = _ec->_w;

    vector<int> fail_block_idx;
    for (int i=0; i<curplacement.size(); i++) {
        for(auto fail_node_id: fail_node_ids) {
            if (curplacement[i] == fail_node_id)
                fail_block_idx.push_back(i);
        }
    }

    // 0. get data structures of ecdag
    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();
    vector<int> ecHeaders = ecdag->getECHeaders();
    vector<int> ecLeaves = ecdag->getECLeaves();
    int intermediate_num = ecNodeMap.size() - ecHeaders.size() - ecLeaves.size();

    // 1. color leave vertices 
    int realLeaves = 0;
    vector<int> avoid_node_ids = fail_node_ids; // 10 12 13?

    cout<<"fail_node_ids:";
    for(int i = 0 ;i < fail_node_ids.size();i++){
        cout<<fail_node_ids[i]<<" ";
    }
    cout << endl;

    int ecq = ecn - eck ;
    int ect =  log(ecw) / log(ecq);
    int ecnu =  (ecn - eck) * ect - ecn;

    for (auto dagidx: ecLeaves) {
        int blkidx = dagidx / ecw;
        int nodeid = -1;
        //18 28 6 17 3 16 25 26 19 0 32 22 14 15

        if (blkidx < ecn) {
            nodeid = curplacement[blkidx];
            if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
                avoid_node_ids.push_back(nodeid);
            realLeaves++;
        }else{
            // nodeid = curplacement[blkidx - ecnu];
            // if(find(avoid_node_ids.begin(), avoid_node_ids.end(), nodeid) == avoid_node_ids.end())
            //     avoid_node_ids.push_back(nodeid);
            // realLeaves++;
        }
        //if(dagidx == 2737) cout<<"!!!"<<nodeid<<endl;
        res.insert(make_pair(dagidx, nodeid));
    }
    cout<<"3863 hang avoid_node_ids:"<<endl;  //0 1 7 5 4 9 8 11 10 6 12 3 2 
    for(auto it:avoid_node_ids){
        cout<<it<<" ";
    }
    cout<<endl;

    // 2. headers (concact sub pkt)

    int repair_node_id;
    vector<int> candidates;
    if (scenario == "standby"){

        int fail_node_count = 0 ;
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();

            //int idx = stripe -> getStripeId() % _standby_size;
            repair_node_id =  _agents_num +  fail_node_count ;//yuhan

            //相同的curplacement[i]的repair_node_id相同
            //repair_node_id =  fail_node_ids[fail_node_count];
            fail_node_count ++;

            res.insert(make_pair(concact_idx, repair_node_id));
            //LOG << "DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
            //cout << "[INFO] stripe " << stripe->getStripeId() << " fail nodeidx is " << (32767-concact_idx)<< " choose new node " << repair_node_id <<"standby"<<endl;
        }
    } 
    else {
        // for each stripe
        for(auto concact_idx: ecdag->_ecConcacts){ 
            // for each concact idx
            // remove source nodes that we should avoid 
            candidates.clear();
            for (int i=0; i<_cluster_size; i++) {
                if(find(curplacement.begin(), curplacement.end(), i) == curplacement.end()){
                    if (find(avoid_node_ids.begin(), avoid_node_ids.end(), i) == avoid_node_ids.end())
                        candidates.push_back(i);
                }
            }
            cout<<"concact_idx"<<concact_idx<<endl;
            //cout<<candidates.size()<<endl;
            // random choose the replacement node for failblock
            for(auto it : candidates){
                cout<<it<<" ";
            }
            cout<<endl;
            repair_node_id = candidates[rand() % candidates.size()];//scatter场景下 随机选 与yuhan不同！
            avoid_node_ids.push_back(repair_node_id);
            //repair_node_id_concact.push_back(repair_node_id);
            res.insert(make_pair(concact_idx, repair_node_id));
            //LOG << "DEBUG concact " << concact_idx << " choose " << repair_node_id << endl;
            //cout << "[INFO] stripe " << stripe->getStripeId() << " fail nodeidx is " << (32767-concact_idx)<< " choose new node " << repair_node_id <<"scatter"<< endl;
        }
    }

    // cout<<"3910 hang avoid_node_ids:"<<endl;  //4
    // for(auto it:avoid_node_ids){
    //     cout<<it<<" ";
    // }
    // cout<<endl;
}


int ParallelSolution::count_common_nodes(const Stripe& s1, const Stripe& s2){
    set<int> set1(s1._nodelist.begin(), s1._nodelist.end());
        set<int> set2(s2._nodelist.begin(), s2._nodelist.end());
        int common = 0;
        for (int num : set1) {
            if (set2.count(num)) common++;
        }
        return common;
}

vector<Stripe*> ParallelSolution::rearrange_stripes(vector<Stripe*> original_list){
    vector<Stripe*> result;
    vector<Stripe*> temp_list = original_list; // 复制列表
    
    while (!temp_list.empty()) {
        Stripe* current = temp_list[0];
        temp_list.erase(temp_list.begin());
        
        if (temp_list.empty()) {
            result.push_back(current);
            break;
        }
        
        // 找重叠最少的Stripe
        int min_common = INT_MAX;
        int best_idx = 0;
        for (int i = 0; i < temp_list.size(); ++i) {
            int common = count_common_nodes(*current, *temp_list[i]);
            if (common < min_common) {
                min_common = common;
                best_idx = i;
            }
        }
        
        result.push_back(current);
        result.push_back(temp_list[best_idx]);
        temp_list.erase(temp_list.begin() + best_idx);
    }
    return result;
}

