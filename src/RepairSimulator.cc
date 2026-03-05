// RepairSimulator.cc
// 修复策略对比模拟器
// 基于Backblaze真实故障trace, 对比三种修复策略:
//   1. Immediate: 每次故障立即修复
//   2. Lazy: 等到容错用尽才修复
//   3. TraceDriven: 根据trace预测下一时间段故障决定是否修复
//
// 用法: ./RepairSimulator code ecn eck ecw num_agents trace_file method scenario
// 改版说明: 利用StripeStore读取集群中真实的数据块分布(如 simulation_40_1000_14_10_4)

#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "common/TraceReader.hh"
#include "common/RepairScheduler.hh"

#include "ec/ECBase.hh"
#include "ec/Clay.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/RDP.hh"
#include "ec/HHXORPlus.hh"
#include "ec/RSCONV.hh"
#include "ec/RSPIPE.hh"

#include "sol/SolutionBase.hh"
#include "sol/ParallelSolution.hh"
#include "sol/RepairBatch.hh"

#include <fstream>
#include <cstdio>
#include <iostream>

using namespace std;

void usage() {
    cout << "Usage: ./RepairSimulator " << endl;
    cout << "   1. code [Clay|RDP|HHXORPlus|BUTTERFLY|RSCONV|RSPIPE]" << endl;
    cout << "   2. ecn" << endl;
    cout << "   3. eck" << endl;
    cout << "   4. ecw" << endl;
    cout << "   5. num_agents (total nodes, e.g. 40)" << endl;
    cout << "   6. trace_file (path to trace file)" << endl;
    cout << "   8. scenario [standby|scatter]" << endl;
    cout << "   9. placement_file (e.g. stripeStore/simulation_40_1000_14_10_4)" << endl;
}

// =================================================================================
// 工具函数：直接从 placement 文件中加载简化的 Stripe 列表
// =================================================================================
vector<Stripe*> loadPlacement(string filepath) {
    vector<Stripe*> stripe_list;
    ifstream infile(filepath);
    string line;
    int stripeid = 0;
    
    if (!infile.is_open()) {
        cerr << "Failed to open placement file: " << filepath << endl;
        return stripe_list;
    }

    while (getline(infile, line)) {
        if (line.empty()) continue;
        vector<string> items = DistUtil::splitStr(line, " ");
        vector<int> nodeidlist;
        for (string s : items) {
            if (!s.empty()) {
                nodeidlist.push_back(atoi(s.c_str()));
            }
        }
        // 创建简化的Stripe，只保留nodeidlist用于仿真评估
        Stripe* curstripe = new Stripe(stripeid++, nodeidlist);
        stripe_list.push_back(curstripe);
    }
    return stripe_list;
}

// =================================================================================
// 工具函数：计算给定一群坏节点，对*一个具体Stripe*产生的修复负载 (load, bdwt)
// =================================================================================
pair<double, double> computeRepairLoadForStripe(string code, int ecn, int eck, int ecw,
                                                Stripe* curStripe,
                                                vector<int> failnodeids,
                                                string scenario, int method, Config* conf, ECBase* ec) {
    int fail_num = failnodeids.size();
    if (fail_num == 0) return {0.0, 0.0};
    
    //cout << "DEBUG: computeRepairLoadForStripe stripe " << curStripe->getStripeId() << " fail_num " << fail_num << endl;
    int real_num_agents = conf->_agents_num + fail_num; // 简化处理，假设修复节点是额外的
    int standby_size = fail_num;

    ParallelSolution* sol = new ParallelSolution(1, standby_size, real_num_agents, method);
    sol->init({curStripe}, ec, code, conf);

    ECDAG* ecdag = curStripe->genRepairECDAG(ec, failnodeids);
    curStripe ->refreshECDAG(ecdag);

    vector<vector<int>> loadtable = vector<vector<int>>(sol->_cluster_size, {0, 0});

    // 区分单故障/多故障
    if (fail_num == 1) {
        unordered_map<int, int> coloring;
        vector<int> placement(real_num_agents);
        for (int i = 0; i < real_num_agents; i++) placement[i] = i;
        //cout<<"DEBUG: computeRepairLoadForStripe stripe " << curStripe->getStripeId() << " failnodeids[0] = " << failnodeids[0] << endl;
        sol->genColoringForSingleFailure(curStripe, coloring, failnodeids[0],
                                          real_num_agents, scenario, placement);
    } else {
        //sol->genColoringForMultipleFailureLevelNewFire(failnodeids, real_num_agents, scenario, 1);
        //cout<<"DEBUG: computeRepairLoadForStripe stripe " << curStripe->getStripeId() << " failnodeids size = " << failnodeids.size() << endl;
        sol->genColoringForMultipleFailureLevelNew(curStripe, failnodeids,
                                                    scenario, loadtable, method);
    }

    double load = curStripe->getLoad() * 1.0 / ecw;
    double bdwt = curStripe->getBdwt() * 1.0 / ecw;

    delete sol;

    return make_pair(load, bdwt);
}

// =================================================================================
// 针对一个策略，运行完整仿真
// 返回统计结果
// =================================================================================
RepairStats runStrategy(int strategy, TraceReader& reader,
                        string code, int ecn, int eck, int ecw,
                        const vector<Stripe*>& stripe_list, ECBase* ec,
                        int num_agents, string scenario, int method,
                        Config* conf, bool verbose) {

    string strategy_names[] = {"Immediate", "Lazy", "TraceDriven"};
    string sname = strategy_names[strategy];

    RepairScheduler scheduler(ecn, eck, ecw);
    reader.reset();

    if (verbose) {
        cout << endl;
        cout << "========================================================" << endl;
        cout << "  Strategy: " << sname << endl;
        cout << "  Code: (" << ecn << "," << eck << ") ecw=" << ecw << endl;
        cout << "  Fault Tolerance: " << (ecn - eck) << endl;
        cout << "========================================================" << endl;
    }

    while (reader.hasCurrentEvent()) {
        TraceEvent* event = reader.getCurrentEvent();

        if (verbose) {
            cout << "[Day " << event->timestamp << "] Failures: ";
            for (int id : event->fail_node_ids) cout << id << " ";
            cout << endl;
        }

        // 添加本轮故障
        bool data_loss = scheduler.addFailures(event->fail_node_ids);

        // ==== 计算哪些Stripe受到了影响，以及是否丢失数据 ====
        vector<int> current_failed_nodes = scheduler.getCurrentFailedNodes();
        int affected_stripes = 0;
        int dataloss_stripes = 0;
        
        for(Stripe* s : stripe_list) {
            vector<int> placement = s->getPlacement();
            int fails_in_stripe = 0;
            for(int nid : current_failed_nodes) {
                if(find(placement.begin(), placement.end(), nid) != placement.end()) {
                    fails_in_stripe++;
                }
            }
            if (fails_in_stripe > 0) affected_stripes++;
            if (fails_in_stripe > (ecn - eck)) dataloss_stripes++;
        }

        if (dataloss_stripes > 0) {
            if (verbose) {
                cout << "  *** DATA LOSS! " << dataloss_stripes << " stripes lost data. ***" << endl;
            }
            scheduler.recordDataLoss(); // count as 1 event
            
            // 必须立刻修复所有目前的故障，以拯救系统
            vector<int> failed = scheduler.executeRepair();
            if (verbose) {
                cout << "  -> Emergency repair: " << failed.size() << " nodes across " << affected_stripes << " stripes" << endl;
            }

            // 对所有受影响的Stripe执行修复代价计算
            for(Stripe* s : stripe_list) {
                vector<int> placement = s->getPlacement();
                vector<int> stripe_failed_nodes;
                for(int nid : failed) {
                    if(find(placement.begin(), placement.end(), nid) != placement.end()) {
                        stripe_failed_nodes.push_back(nid);
                    }
                }
                if (stripe_failed_nodes.size() > 0 && stripe_failed_nodes.size() <= (ecn - eck)) {
                    auto cost = computeRepairLoadForStripe(code, ecn, eck, ecw, s, stripe_failed_nodes, scenario, method, conf, ec);
                    scheduler.recordRepairLoad(cost.first, cost.second);
                }
            }

            reader.advance();
            continue;
        }

        // 根据策略决策
        RepairDecision decision;
        switch (strategy) {
            case 0: decision = scheduler.decideImmediate(); break;
            case 1: decision = scheduler.decideLazy(); break;
            case 2: decision = scheduler.decideTraceDriven(reader.peekNextEvent()); break;
            default: decision = REPAIR_IMMEDIATE;
        }

        if (verbose) {
            scheduler.printStatus(sname);
        }

        if (decision == REPAIR_IMMEDIATE) {
            vector<int> failed = scheduler.executeRepair();
            if (verbose) {
                cout << "  -> REPAIR: " << failed.size() << " nodes [";
                for (int id : failed) cout << id << " ";
                cout << "] affected_stripes=" << affected_stripes << endl;
            }

            // 对每个Stripe分别计算代价
            for(Stripe* s : stripe_list) {
                vector<int> placement = s->getPlacement();
                vector<int> stripe_failed_nodes;
                for(int nid : failed) {
                    if(find(placement.begin(), placement.end(), nid) != placement.end()) {
                        stripe_failed_nodes.push_back(nid);
                    }
                }
                if(stripe_failed_nodes.size() > 0 && stripe_failed_nodes.size() <= (ecn-eck)) {
                    auto cost = computeRepairLoadForStripe(code, ecn, eck, ecw, s, stripe_failed_nodes, scenario, method, conf, ec);
                    scheduler.recordRepairLoad(cost.first, cost.second);
                }
            }
        } else {
            if (verbose) {
                cout << "  -> LAZY: skip repair, " << current_failed_nodes.size() << " nodes failed currently." << endl;
            }
        }

        reader.advance();
    }

    // 收尾: 还有故障节点则做最后一次修复
    int final_failures = scheduler.getCurrentFailureCount();
    if (final_failures > 0) {
        if (verbose) {
            cout << "[END] Final repair: " << final_failures << " remaining failures" << endl;
        }
        vector<int> failed = scheduler.executeRepair();
        for(Stripe* s : stripe_list) {
            vector<int> placement = s->getPlacement();
            vector<int> stripe_failed_nodes;
            for(int nid : failed) {
                if(find(placement.begin(), placement.end(), nid) != placement.end()) {
                    stripe_failed_nodes.push_back(nid);
                }
            }
            if(stripe_failed_nodes.size() > 0 && stripe_failed_nodes.size() <= (ecn-eck)) {
                auto cost = computeRepairLoadForStripe(code, ecn, eck, ecw, s, stripe_failed_nodes, scenario, method, conf, ec);
                scheduler.recordRepairLoad(cost.first, cost.second);
            }
        }
    }

    RepairStats stats = scheduler.getStats();

    if (verbose) {
        cout << endl;
        cout << "--------- " << sname << " Results ---------" << endl;
        cout << "  Repair rounds:       " << stats.total_repair_rounds << endl;
        cout << "  Total load:          " << stats.total_repair_load << " blocks" << endl;
        cout << "  Total bandwidth:     " << stats.total_repair_bdwt << " blocks" << endl;
        cout << "  Data loss events:    " << stats.data_loss_events << endl;
        cout << "  Max concurrent fail: " << stats.max_concurrent_failures << endl;
        cout << "-------------------------------------------" << endl;
    }

    return stats;
}

int main(int argc, char** argv) {

    if (argc < 10) {
        usage();
        return 0;
    }

    string code = string(argv[1]);
    int ecn = atoi(argv[2]);
    int eck = atoi(argv[3]);
    int ecw = atoi(argv[4]);
    int num_agents = atoi(argv[5]);
    string trace_file = string(argv[6]);
    int method = atoi(argv[7]);
    string scenario = string(argv[8]);
    string placement_file = string(argv[9]);

    cout << "============================================" << endl;
    cout << "  Trace-Driven Repair Strategy Simulator" << endl;
    cout << "============================================" << endl;
    cout << "Code: " << code << " (" << ecn << "," << eck << ") ecw=" << ecw << endl;
    cout << "Nodes: " << num_agents << endl;
    cout << "Trace: " << trace_file << endl;
    cout << "Fault tolerance: " << (ecn - eck) << endl;
    cout << "Scenario: " << scenario << endl;
    cout << "Method: " << method << endl;

    // 临时伪造Config供部分类的初始化（不用改原有大量代码）
    // 为了兼容，配置一个基础参数即可
    string fake_conf = "conf/sysSetting.xml"; // 借用一下以通过构造函数
    Config* conf = new Config(fake_conf);
    conf->_agents_num = num_agents;

    // 加载trace
    TraceReader reader;
    if (!reader.loadTrace(trace_file)) {
        cerr << "Failed to load trace file: " << trace_file << endl;
        delete conf;
        return 1;
    }

    // 从文件中加载Stripe Placement分布
    // ==================
    vector<Stripe*> stripe_list = loadPlacement(placement_file);
    if (stripe_list.empty()) {
        cerr << "No stripes loaded from " << placement_file << endl;
        delete conf;
        return 1;
    }

    // ==================
    // 实例化EC对象 (共用，避免重复创建)
    // ==================
    vector<string> param;
    ECBase* ec;
    if (code == "Clay") {
        ec = new Clay(ecn, eck, ecw, {to_string(ecn - 1)});
    } else if (code == "RDP") {
        ec = new RDP(ecn, eck, ecw, param);
    } else if (code == "HHXORPlus") {
        ec = new HHXORPlus(ecn, eck, ecw, param);
    } else if (code == "BUTTERFLY") {
        ec = new BUTTERFLY(ecn, eck, ecw, param);
    } else if (code == "RSCONV") {
        ec = new RSCONV(ecn, eck, ecw, param);
    } else if (code == "RSPIPE") {
        ec = new RSPIPE(ecn, eck, ecw, param);
    } else {
        cerr << "Non-supported code: " << code << endl;
        delete conf;
        return 1;
    }

    // ====== 详细运行三种策略 ======
    RepairStats stats[3];
    string names[] = {"Immediate", "Lazy", "TraceDriven"};

    for (int s = 0; s < 3; s++) {
        cout<<"Running strategy: " << names[s] << "..." << endl;
        stats[s] = runStrategy(s, reader, code, ecn, eck, ecw,
                               stripe_list, ec, num_agents, scenario, method, conf, true);
    }

    // ====== 对比总结表 ======
    cout << endl;
    cout << "========================================================" << endl;
    cout << "              COMPARISON SUMMARY" << endl;
    cout << "  Code: " << code << " (" << ecn << "," << eck << ")" << endl;
    cout << "  Trace events: " << reader.getEventCount() << endl;
    cout << "========================================================" << endl;
    printf("%-14s | %6s | %10s | %10s | %8s | %7s\n",
           "Strategy", "Rounds", "Load(blks)", "Bdwt(blks)", "DataLoss", "MaxFail");
    printf("%-14s-|-%6s-|-%10s-|-%10s-|-%8s-|-%7s\n",
           "--------------", "------", "----------", "----------", "--------", "-------");
    for (int s = 0; s < 3; s++) {
        printf("%-14s | %6d | %10.2f | %10.2f | %8d | %7d\n",
               names[s].c_str(),
               stats[s].total_repair_rounds,
               stats[s].total_repair_load,
               stats[s].total_repair_bdwt,
               stats[s].data_loss_events,
               stats[s].max_concurrent_failures);
    }
    cout << "========================================================" << endl;

    // 计算Lazy和TraceDriven相比Immediate的修复流量节省
    if (stats[0].total_repair_load > 0) {
        double lazy_save = (1.0 - stats[1].total_repair_load / stats[0].total_repair_load) * 100;
        double trace_save = (1.0 - stats[2].total_repair_load / stats[0].total_repair_load) * 100;
        cout << endl;
        cout << "Load savings vs Immediate:" << endl;
        printf("  Lazy:         %.1f%% (data loss: %d)\n", lazy_save, stats[1].data_loss_events);
        printf("  TraceDriven:  %.1f%% (data loss: %d)\n", trace_save, stats[2].data_loss_events);
    }

    cout << endl;
    // for(Stripe* s : stripe_list) {
    //     delete s;
    // }

   if (conf)
       delete conf;
    return 0;
}
