// RepairSimulator.cc
// 修复策略对比模拟器
// 基于Backblaze真实故障trace, 对比三种修复策略:
//   1. Immediate: 每次故障立即修复
//   2. Lazy: 等到容错用尽才修复
//   3. TraceDriven: 根据trace预测下一时间段故障决定是否修复
//
// 用法: ./RepairSimulator code ecn eck ecw num_agents trace_file method scenario

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

using namespace std;

void usage() {
    cout << "Usage: ./RepairSimulator " << endl;
    cout << "   1. code [Clay|RDP|HHXORPlus|BUTTERFLY|RSCONV|RSPIPE]" << endl;
    cout << "   2. ecn" << endl;
    cout << "   3. eck" << endl;
    cout << "   4. ecw" << endl;
    cout << "   5. num_agents (total nodes, e.g. 40)" << endl;
    cout << "   6. trace_file (path to trace file)" << endl;
    cout << "   7. method (0: default)" << endl;
    cout << "   8. scenario [standby|scatter]" << endl;
}

// 计算一次修复的负载(load, bdwt)
// 复用Multiple_Failure.cc中的模式: 创建Stripe -> 生成ECDAG -> 着色 -> 获取load/bdwt
pair<double, double> computeRepairLoad(string code, int ecn, int eck, int ecw,
                                        vector<int> failnodeids,
                                        string scenario, int method, Config* conf) {
    int fail_num = failnodeids.size();
    // 与Multiple_Failure.cc一致: num_agents = ecn + fail_num
    int real_num_agents = ecn + fail_num;
    int standby_size = fail_num;

    // 创建一个标准placement的条带 (node 0 ~ ecn-1)
    vector<int> nodeidlist;
    for (int i = 0; i < ecn; i++) {
        nodeidlist.push_back(i);
    }
    Stripe* currStripe = new Stripe(0, nodeidlist);

    // 创建EC对象
    ECBase* ec;
    vector<string> param;
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
        delete currStripe;
        return make_pair(0.0, 0.0);
    }

    // 创建ParallelSolution, 与Multiple_Failure.cc一致
    ParallelSolution* sol = new ParallelSolution(1, standby_size, real_num_agents, method);
    sol->init({currStripe}, ec, code, conf);

    vector<vector<int>> loadtable = vector<vector<int>>(sol->_cluster_size, {0, 0});

    // 与Multiple_Failure.cc一致: 单故障和多故障走不同路径
    if (fail_num == 1) {
        unordered_map<int, int> coloring;
        vector<int> placement(real_num_agents);
        for (int i = 0; i < real_num_agents; i++) placement[i] = i;
        sol->genColoringForSingleFailure(currStripe, coloring, failnodeids[0],
                                          real_num_agents, scenario, placement);
    } else {
        sol->genColoringForMultipleFailureLevelNew(currStripe, failnodeids,
                                                    scenario, loadtable, method);
    }

    double load = currStripe->getLoad() * 1.0 / ecw;
    double bdwt = currStripe->getBdwt() * 1.0 / ecw;

    delete currStripe;
    delete ec;
    delete sol;

    return make_pair(load, bdwt);
}

// 对某一策略运行完整仿真, 返回统计结果
RepairStats runStrategy(int strategy, TraceReader& reader,
                        string code, int ecn, int eck, int ecw,
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

        if (data_loss) {
            if (verbose) {
                cout << "  *** DATA LOSS! Failures ("
                     << scheduler.getCurrentFailureCount()
                     << ") exceed fault tolerance ("
                     << scheduler.getFaultTolerance() << ") ***" << endl;
            }
            scheduler.recordDataLoss();

            // 数据丢失后必须修复
            vector<int> failed = scheduler.executeRepair();
            if (verbose) {
                cout << "  -> Emergency repair: " << failed.size() << " nodes" << endl;
            }

            // 计算修复负载
            vector<int> effective;
            for (int nid : failed) {
                if (nid < ecn) effective.push_back(nid);
            }
            if (!effective.empty() && (int)effective.size() <= (ecn - eck)) {
                auto cost = computeRepairLoad(code, ecn, eck, ecw,
                                              effective, scenario, method, conf);
                scheduler.recordRepairLoad(cost.first, cost.second);
                if (verbose) {
                    cout << "  -> load=" << cost.first
                         << ", bdwt=" << cost.second << " blocks" << endl;
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
                cout << "]" << endl;
            }

            // 计算修复负载
            vector<int> effective;
            for (int nid : failed) {
                if (nid < ecn) effective.push_back(nid);
            }
            if (!effective.empty() && (int)effective.size() <= (ecn - eck)) {
                auto cost = computeRepairLoad(code, ecn, eck, ecw,
                                              effective, scenario, method, conf);
                scheduler.recordRepairLoad(cost.first, cost.second);
                if (verbose) {
                    cout << "  -> load=" << cost.first
                         << ", bdwt=" << cost.second << " blocks" << endl;
                }
            }
        } else {
            if (verbose) {
                cout << "  -> LAZY: skip repair ("
                     << scheduler.getCurrentFailureCount() << "/"
                     << scheduler.getFaultTolerance() << ")" << endl;
            }
        }

        reader.advance();
    }

    // 收尾: 还有故障节点则做最后一次修复
    if (scheduler.getCurrentFailureCount() > 0) {
        if (verbose) {
            cout << "[END] Final repair: " << scheduler.getCurrentFailureCount()
                 << " remaining failures" << endl;
        }
        vector<int> failed = scheduler.executeRepair();
        vector<int> effective;
        for (int nid : failed) {
            if (nid < ecn) effective.push_back(nid);
        }
        if (!effective.empty() && (int)effective.size() <= (ecn - eck)) {
            auto cost = computeRepairLoad(code, ecn, eck, ecw,
                                          effective, scenario, method, conf);
            scheduler.recordRepairLoad(cost.first, cost.second);
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

    if (argc < 9) {
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

    cout << "============================================" << endl;
    cout << "  Trace-Driven Repair Strategy Simulator" << endl;
    cout << "============================================" << endl;
    cout << "Code: " << code << " (" << ecn << "," << eck << ") ecw=" << ecw << endl;
    cout << "Nodes: " << num_agents << endl;
    cout << "Trace: " << trace_file << endl;
    cout << "Fault tolerance: " << (ecn - eck) << endl;
    cout << "Scenario: " << scenario << endl;
    cout << "Method: " << method << endl;

    string configpath = "conf/sysSetting.xml";
    Config* conf = new Config(configpath);

    // 加载trace
    TraceReader reader;
    if (!reader.loadTrace(trace_file)) {
        cerr << "Failed to load trace file: " << trace_file << endl;
        delete conf;
        return 1;
    }

    // ====== 详细运行三种策略 ======
    RepairStats stats[3];
    string names[] = {"Immediate", "Lazy", "TraceDriven"};

    for (int s = 0; s < 3; s++) {
        stats[s] = runStrategy(s, reader, code, ecn, eck, ecw,
                               num_agents, scenario, method, conf, true);
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
    delete conf;
    return 0;
}
