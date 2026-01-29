#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/ECBase.hh"
#include "ec/Clay.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/HHXORPlus.hh"
#include "ec/RDP.hh"
#include "sol/SolutionBase.hh"
#include "sol/ParallelSolution.hh"
#include <dirent.h>
#include <fstream>


using namespace std;

void usage() {
    cout << "usage: ./Simulation" << endl;
    cout << "   1. placement file path" << endl;
    cout << "   2. number of agents" << endl;
    cout << "   3. number of stripes" << endl;
    cout << "   4. code [Clay]" << endl;
    cout << "   5. ecn" << endl;
    cout << "   6. eck" << endl;
    cout << "   7, ecw" << endl;
    cout << "   8. scenario [standby|scatter]" << endl;
    cout << "   9. standbysize" << endl;
    cout << "   10. batchsize [3]" << endl;
    cout << "   11. method [3]" << endl;
    cout << "   12. failnodeid_size" << endl;
    cout << "   13. failnodeids[0,1,2,3]" << endl;
}
// // 新增：计算两个Stripe节点列表的重叠数量（适配真实Stripe类）
// int count_common_nodes(const Stripe& s1, const Stripe& s2) {
//     set<int> set1(s1._nodelist.begin(), s1._nodelist.end());
//     set<int> set2(s2._nodelist.begin(), s2._nodelist.end());
//     int common = 0;
//     for (int num : set1) {
//         if (set2.count(num)) common++;
//     }
//     return common;
// }

// // 新增：对Stripe列表做差异化两两分组重排
// vector<Stripe*> rearrange_stripes(vector<Stripe*> original_list) {
//     vector<Stripe*> result;
//     vector<Stripe*> temp_list = original_list; // 复制列表
    
//     while (!temp_list.empty()) {
//         Stripe* current = temp_list[0];
//         temp_list.erase(temp_list.begin());
        
//         if (temp_list.empty()) {
//             result.push_back(current);
//             break;
//         }
        
//         // 找重叠最少的Stripe
//         int min_common = INT_MAX;
//         int best_idx = 0;
//         for (int i = 0; i < temp_list.size(); ++i) {
//             int common = count_common_nodes(*current, *temp_list[i]);
//             if (common < min_common) {
//                 min_common = common;
//                 best_idx = i;
//             }
//         }
        
//         result.push_back(current);
//         result.push_back(temp_list[best_idx]);
//         temp_list.erase(temp_list.begin() + best_idx);
//     }
//     return result;
// }
// 计算两个Stripe节点列表的重叠数量（保留原有功能）
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
    
    // if (argc != 11) {
    //     usage();
    //     return 0;
    // }

    string filepath = argv[1];
    int num_agents = atoi(argv[2]);
    int num_stripes = atoi(argv[3]);
    string code = argv[4];
    int ecn = atoi(argv[5]);
    int eck = atoi(argv[6]);
    int ecw = atoi(argv[7]);
    string scenario = argv[8];
    int standby_size = atoi(argv[9]);
    int batchsize = atoi(argv[10]);
    int method = atoi(argv[11]);
//    int fnid = atoi(argv[10]);
    int failnodeid_size = atoi(argv[12]);
    if (argc != failnodeid_size + 13) {
        std::cout << "Invalid number of elements provided." << std::endl;
        return 1;
    }
    vector<int> failnodeids;
    for (int i = 0; i < failnodeid_size; ++i) {
        failnodeids.push_back(std::atoi(argv[i + 13]));
    }

    string config_path = "conf/sysSetting.xml";
    Config* conf = new Config(config_path);

    // 0. read block placement
    vector<Stripe*> stripelist;

    ifstream infile(filepath);
    for (int stripeid=0; stripeid<num_stripes; stripeid++) {
        string line;
        getline(infile, line);
        vector<string> items = DistUtil::splitStr(line, " ");
        vector<int> nodeidlist;
        for (int i=0; i<ecn; i++) {
            int nodeid = atoi(items[i].c_str());
            nodeidlist.push_back(nodeid);
        }
        Stripe* curstripe = new Stripe(stripeid, nodeidlist);
        stripelist.push_back(curstripe);
    }
    infile.close();

    // ========== 第二步：新增 - 对stripelist做差异化重排,并重新读取nodeid 和 nodeidlist ==========
    vector<Stripe*> optimized_stripelist = rearrange_stripes(stripelist,batchsize);
    cout<<"差异化重排！！！！"<<endl;

    vector<vector<int>> rearranged_nodeidlists;

    for (int stripeid=0; stripeid<num_stripes; stripeid++) {
        
       Stripe* cur_stripe = optimized_stripelist[stripeid];
        vector<int> nodeidlist = cur_stripe->_nodelist; // 核心：直接获取nodeidlist
        rearranged_nodeidlists.push_back(nodeidlist);
    }
    
    // 调试输出验证
    cout << "\n重排后的节点列表总数：" << rearranged_nodeidlists.size() << endl;
    for (int i = 0; i < rearranged_nodeidlists.size(); ++i) {
        cout << "条带" << i << ": ";
        for (int n : rearranged_nodeidlists[i]) cout << n << " ";
        cout << endl;
    }
    

        // // ========== 第三步：可选 - 将重排后的分布写入新文件 ==========
        // ofstream outfile(filepath + "_optimized");
        // if (outfile.is_open()) {
        //     for (Stripe* s : optimized_stripelist) {
        //         vector<int> nodes = s->getPlacement();
        //         for (int i = 0; i < nodes.size(); ++i) {
        //             if (i > 0) outfile << " ";
        //             outfile << nodes[i];
        //         }
        //         outfile << endl;
        //     }
        //     outfile.close();
        //     cout << "Generated optimized stripe file: " << filepath + "_optimized" << endl;
        // }


    struct timeval time1,time2,time3;
    // 1. init a solution
    ECBase* ec;

    vector<string> param;
    if (code == "Clay") {
        ec = new Clay(ecn, eck, ecw, {to_string(ecn-1)});
    } else if (code == "RDP") {
        ec = new RDP(ecn, eck, ecw, param);
    } else if (code == "HHXORPlus") {
        ec = new HHXORPlus(ecn, eck, ecw, param);
    } else if (code == "BUTTERFLY") {
        ec = new BUTTERFLY(ecn, eck, ecw, param);
    } else {
        cout << "Non-supported code!" << endl;
        return 0;
    }
    

    //string sol_param = to_string(batchsize);
    SolutionBase* sol = new ParallelSolution(batchsize, standby_size, num_agents ,method);
    sol->init(optimized_stripelist, ec, code, conf);
    //sol->init(stripelist, ec, code, conf);

    gettimeofday(&time1,NULL);
    // 2. create a thread to generate repair batches
    sol->genRepairBatches(failnodeid_size, failnodeids, num_agents, scenario, true);
    gettimeofday(&time2,NULL);
    
    // 3. get repair batches
    vector<RepairBatch*> repairbatches = sol->getRepairBatches();

    cout << "repairbatches.size: " << repairbatches.size() << endl;

    // 4. get statistics for each batch
    int overall_load = 0;
    int overall_bdwt = 0;
    for (int batchid=0; batchid<repairbatches.size(); batchid++) {
        RepairBatch* curbatch = repairbatches[batchid];
        int load = curbatch->getLoad();
        overall_load += load;
        overall_bdwt += curbatch->getBdwt();
        
        for (auto stripe : curbatch->getStripeList()) {
            cout << "STRIPE " << stripe->getStripeId() << endl;
        //    stripe->dumpTrans(num_agents + standby_size);
            stripe->dumpLoad(num_agents + standby_size);
        }

        // curbatch->dumpLoad(num_agents+standby_size);
        // LOG <<"BATCH "<< curbatch ->getBatchId()<<endl;
        // LOG <<"BATCH load = " << load << endl;


    }
    double duration = DistUtil::duration(time1, time2);
    cout << "duration = " << duration << endl;
    cout << "[RET] overall load: "  << 1.0*overall_load/ecw << " blocks" << endl;
    cout << "[RET] overall bdwt: "  << 1.0*overall_bdwt/ecw << " blocks" << endl;
    LOG << "overall load: "  << 1.0*overall_load/ecw << " blocks" << endl;
    LOG << "overall bdwt: "  << 1.0*overall_bdwt/ecw << " blocks" << endl;

     // 内存释放（重要）
  
    for (Stripe* s : stripelist) {
        delete s;
    }
    stripelist.clear();
    optimized_stripelist.clear();

}
    // int overall_load = 0;
    // int overall_bdwt = 0;
    // struct timeval time1, time2, time3, time4;
    // double latency = 0;
    // vector<RepairBatch*> batch_list;
    // vector<double> latency_list;

    // vector<double> load_list;
    
    // while (sol->hasNext()) {
    //     gettimeofday(&time1, NULL);
    //     RepairBatch* curbatch = sol->getRepairBatchFromQueue();
    //     batch_list.push_back(curbatch);
    //     gettimeofday(&time2, NULL);
    //     latency += DistUtil::duration(time1, time2);
    //     latency_list.push_back(DistUtil::duration(time1,time2));

    //     int load = curbatch->getLoad();
        
    //     load_list.push_back(load);

    //     overall_load += load;
    //     LOG<<"130hang :overall_load:"<<overall_load<<endl;
    //     overall_bdwt += curbatch->getBdwt();
    //     sleep(1);
    // }
    // for(int i = 0 ; i < batch_list.size(); i++)
    // {
    //     batch_list[i]->dump(num_agents + standby_size);
    //     cout << "get batch duraiton: " << latency_list[i] << endl; 
    //     LOG << "138 hang get batch load: " << load_list[i] << endl; 

    // }
    // // join
    // genthread.join();
    // LOG << "overall load: " <<  overall_load << " subblocks" << endl;
    // LOG << "overall bdwt: " <<  overall_bdwt << " subblocks" << endl;
    // LOG << "get repair batch latency: " << latency << " ms" << endl;
