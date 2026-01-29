//#include "common/CmdDistributor.hh"
#include "common/Config.hh"
#include "common/Coordinator.hh"
#include "common/StripeStore.hh"

#include "inc/include.hh"

#include "sol/RepairBatch.hh"

using namespace std;

void usage() {
    cout << "Usage: ./ParaCoordinator " << endl;
    cout << "   1. method [centralize|offline|parallel]" << endl;
    cout << "   2. scenario [standby|scatter]" << endl;
    cout << "   3. failnode num [3]" << endl;
    cout << "   4. failnode ids [0,1,2]" << endl;
}

int count_common_nodes(const Stripe& s1, const Stripe& s2) {
    set<int> set1(s1._nodelist.begin(), s1._nodelist.end());
    set<int> set2(s2._nodelist.begin(), s2._nodelist.end());
    int common = 0;
    for (int num : set1) {
        if (set2.count(num)) common++;
    }
    return common;
}

// 计算单个条带与一组条带的平均重叠度
double calculate_avg_common_with_group(const Stripe& target, const vector<Stripe*>& group) {
    if (group.empty()) return 0.0; // 空组无重叠
    
    int total_common = 0;
    for (Stripe* s : group) {
        total_common += count_common_nodes(target, *s);
    }
    return static_cast<double>(total_common) / group.size(); // 返回平均重叠数
}

// 扩展：支持batchsize=1（不重排）、batchsize>=2（按组重排）
vector<Stripe*> rearrange_stripes(vector<Stripe*> original_list, int batchsize) {
    // 输入校验：batchsize<1时修正为1
    if (batchsize < 1) {
        cerr << "警告：batchsize不能小于1，已自动设为1" << endl;
        batchsize = 1;
    }

    // 核心逻辑：batchsize=1时直接返回原列表（不调整顺序）
    if (batchsize == 1) {
        cout << "batchsize=1，不调整条带顺序，直接返回原始列表" << endl;
        return original_list;
    }

    // 空列表直接返回
    if (original_list.empty()) {
        cerr << "警告：原始条带列表为空" << endl;
        return {};
    }

    vector<Stripe*> result;
    vector<Stripe*> temp_list = original_list; // 复制原始列表，避免修改原数据

    while (!temp_list.empty()) {
        vector<Stripe*> current_group; // 当前组的条带列表
        
        // 1. 选第一个条带作为组基准
        Stripe* base_stripe = temp_list[0];
        current_group.push_back(base_stripe);
        temp_list.erase(temp_list.begin());

        // 2. 继续选择条带，直到凑够batchsize个或列表为空
        while (current_group.size() < batchsize && !temp_list.empty()) {
            double min_avg_common = 100000;
            int best_idx = 0;

            // 遍历剩余条带，找与当前组平均重叠度最小的条带
            for (int i = 0; i < temp_list.size(); ++i) {
                double avg_common = calculate_avg_common_with_group(*temp_list[i], current_group);
                if (avg_common < min_avg_common) {
                    min_avg_common = avg_common;
                    best_idx = i;
                }
            }

            // 将最优条带加入当前组，并从剩余列表移除
            current_group.push_back(temp_list[best_idx]);
            temp_list.erase(temp_list.begin() + best_idx);
        }

        // 3. 将当前组的所有条带加入结果
        result.insert(result.end(), current_group.begin(), current_group.end());
        
        // 调试输出：打印每组的条带ID和重叠度（可选）
        cout << "生成分组：";
        for (Stripe* s : current_group) {
            cout << s->getStripeId() << " ";
        }
        cout << " | 组内条带数：" << current_group.size() << endl;
    }

    return result;
}


int main(int argc, char** argv) {

    if (argc < 4) {
        usage();
        return 0;
    }
    string method = string(argv[1]);
    string scenario = string(argv[2]);
    int failnodeid_size = atoi(argv[3]);
    if (argc != failnodeid_size + 4) {
        std::cout << "Invalid number of elements provided." << std::endl;
        return 1;
    }
    vector<int> failnodeids;
    for (int i = 0; i < failnodeid_size; ++i) {
        failnodeids.push_back(std::atoi(argv[i + 4]));
    }

    // cout << "Multiple failures: !!!";
    // for(auto it  : failnodeids){
    //     cout << " " << it;
    // }
    // cout << endl;

    // cout<<"!!!"<<endl;

    string configpath = "conf/sysSetting.xml";
    cout<<"Config begin"<<endl;
    Config* conf = new Config(configpath);
    cout <<"Config end"<<endl;

    int ecn = conf->_ecn;
    int eck = conf->_eck;
    int ecw = conf->_ecw;

    int batchsize = conf->_batch_size;
    
    // create stripestore
    // TODO: need to add recover from backup
    StripeStore* ss = new StripeStore(conf); 

    //ZhangJL add begin

    // ========== 第二步：新增 - 对stripelist做差异化重排,并重新读取nodeid 和 nodeidlist ==========
    vector<Stripe*> optimized_stripelist = rearrange_stripes(ss->getStripeList(),batchsize);

    vector<vector<int>> rearranged_nodeidlists;

    for (int stripeid=0; stripeid<ss->getStripeList().size(); stripeid++) {
       Stripe* cur_stripe = optimized_stripelist[stripeid];
        vector<int> nodeidlist = cur_stripe->_nodelist; // 核心：直接获取nodeidlist
        rearranged_nodeidlists.push_back(nodeidlist);
    }
    //将rearranged_nodeidlists赋值给ss中的每个Stripe
    ss->_stripe_list = optimized_stripelist;

    //ZhangJL add end

    // coordinator
    Coordinator* coor = new Coordinator(conf, ss);

    cout<<"MultipleCoor::initRepair start"<<endl;

    coor->initRepair(method, scenario, failnodeids);
    
    cout<<"MultipleCoor::initRepair ends"<<endl;

    struct timeval time1, time2, time3, time4, time5;


    // parallel
    double latency = 0;
    vector<double> batchtime;
    
    thread genThread = thread([=]{coor->genRepairSolutionAsync();});
    
    cout<<"MultipleCoor::genRepairSolutionSaync() end"<<endl;
    
    coor->repair();
    cout<<"MultipleCoor::repair end"<<endl;

    genThread.join();

   // clean
   // clean coordinator
   if (coor)
       delete coor;
   // clean stripe store
   if (ss)
       delete ss;
   // clean conf
   if (conf)
       delete conf;

    return 0;
}
