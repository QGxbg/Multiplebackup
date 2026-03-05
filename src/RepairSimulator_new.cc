// RepairSimulator_new.cc
// 扩展版修复策略对比模拟器
//
// 在原有三策略基础上新增：
//   Strategy 3: RiskAwarePartial  (风险量化 + 部分修复 + 自适应窗口)
//   Strategy 4: StripeDifferentiated (Stripe级别差异化修复)
//   Strategy 5: Combined (1+2+3完整组合)
//
// 用法: ./RepairSimulator_new code ecn eck ecw num_agents trace_file method scenario placement_file

#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "common/TraceReader.hh"
#include "common/RepairScheduler.hh"
#include "RepairScheduler_new.hh"   // 新增调度器

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
#include <iomanip>

using namespace std;

// =================================================================================
// 复用原有的辅助函数（保持不变）
// =================================================================================

vector<Stripe*> loadPlacement(string filepath) {
    vector<Stripe*> stripe_list;
    ifstream infile(filepath);
    if (!infile.is_open()) {
        cerr << "Failed to open placement file: " << filepath << endl;
        return stripe_list;
    }
    string line;
    int stripeid = 0;
    while (getline(infile, line)) {
        if (line.empty()) continue;
        vector<string> items = DistUtil::splitStr(line, " ");
        vector<int> nodeidlist;
        for (string s : items)
            if (!s.empty()) nodeidlist.push_back(atoi(s.c_str()));
        stripe_list.push_back(new Stripe(stripeid++, nodeidlist));
    }
    return stripe_list;
}

pair<double, double> computeRepairLoadForStripe(string code, int ecn, int eck, int ecw,
                                                Stripe* curStripe,
                                                vector<int> failnodeids,
                                                string scenario, int method,
                                                Config* conf, ECBase* ec) {
    int fail_num = failnodeids.size();
    if (fail_num == 0) return {0.0, 0.0};

    int real_num_agents = conf->_agents_num + fail_num;
    int standby_size = fail_num;

    ParallelSolution* sol = new ParallelSolution(1, standby_size, real_num_agents, method);
    sol->init({curStripe}, ec, code, conf);

    ECDAG* ecdag = curStripe->genRepairECDAG(ec, failnodeids);
    curStripe->refreshECDAG(ecdag);

    vector<vector<int>> loadtable = vector<vector<int>>(sol->_cluster_size, {0, 0});

    if (fail_num == 1) {
        unordered_map<int, int> coloring;
        vector<int> placement(real_num_agents);
        for (int i = 0; i < real_num_agents; i++) placement[i] = i;
        sol->genColoringForSingleFailure(curStripe, coloring, failnodeids[0],
                                         real_num_agents, scenario, placement);
    } else {
        sol->genColoringForMultipleFailureLevelNew(curStripe, failnodeids,
                                                    scenario, loadtable, method);
    }

    double load = curStripe->getLoad() * 1.0 / ecw;
    double bdwt = curStripe->getBdwt() * 1.0 / ecw;
    delete sol;
    return {load, bdwt};
}

// =================================================================================
// 辅助：针对一组节点，批量计算所有受影响stripe的修复代价
// =================================================================================
void applyRepairCost(const vector<int>& repaired_nodes,
                     const vector<Stripe*>& stripe_list,
                     string code, int ecn, int eck, int ecw,
                     string scenario, int method, Config* conf, ECBase* ec,
                     RepairSchedulerNew& scheduler)
{
    for (Stripe* s : stripe_list) {
        const auto& placement = s->getPlacement();
        vector<int> stripe_failed;
        for (int nid : repaired_nodes) {
            if (find(placement.begin(), placement.end(), nid) != placement.end())
                stripe_failed.push_back(nid);
        }
        if (!stripe_failed.empty() && (int)stripe_failed.size() <= (ecn - eck)) {
            auto cost = computeRepairLoadForStripe(code, ecn, eck, ecw, s,
                                                    stripe_failed, scenario,
                                                    method, conf, ec);
            scheduler.recordRepairLoad(cost.first, cost.second);
        }
    }
}

// =================================================================================
// Stripe级别数据丢失检测
// 只有某个stripe内的故障块数 > (ecn-eck) 才算真正数据丢失
// 返回：有数据丢失的stripe数量（0表示无数据丢失）
// =================================================================================
int checkStripeDataLoss(const vector<int>& current_failed_nodes,
                        const vector<Stripe*>& stripe_list,
                        int ecn, int eck)
{
    int tolerance = ecn - eck;
    int loss_count = 0;
    for (Stripe* s : stripe_list) {
        const vector<int>& placement = s->getPlacement();
        int fails_in_stripe = 0;
        for (int nid : current_failed_nodes) {
            if (find(placement.begin(), placement.end(), nid) != placement.end())
                fails_in_stripe++;
        }
        if (fails_in_stripe > tolerance)
            loss_count++;
    }
    return loss_count;
}

// =================================================================================
// 通用的数据丢失应急处理
// =================================================================================
void handleDataLoss(RepairSchedulerNew& scheduler,
                    const vector<Stripe*>& stripe_list,
                    string code, int ecn, int eck, int ecw,
                    string scenario, int method, Config* conf, ECBase* ec,
                    bool verbose, int loss_stripes)
{
    scheduler.recordDataLoss();
    vector<int> failed = scheduler.executeFullRepair();
    if (verbose)
        cout << "  *** DATA LOSS! " << loss_stripes << " stripes lost. "
             << "Emergency repair: " << failed.size() << " nodes" << endl;
    applyRepairCost(failed, stripe_list, code, ecn, eck, ecw, scenario, method, conf, ec, scheduler);
}

// =================================================================================
// 原有三种策略（适配新调度器接口）
// =================================================================================
RepairStats runOriginalStrategy(int strategy, TraceReader& reader,
                                string code, int ecn, int eck, int ecw,
                                const vector<Stripe*>& stripe_list, ECBase* ec,
                                int num_agents, string scenario, int method,
                                Config* conf, bool verbose)
{
    string names[] = {"Immediate", "Lazy", "TraceDriven"};
    RepairSchedulerNew scheduler(ecn, eck, ecw);
    reader.reset();

    if (verbose) {
        cout << "\n======================================================" << endl;
        cout << "  Strategy: " << names[strategy] << endl;
        cout << "======================================================" << endl;
    }

    while (reader.hasCurrentEvent()) {
        TraceEvent* event = reader.getCurrentEvent();
        scheduler.addFailures(event->fail_node_ids);
        vector<int> cur_failed = scheduler.getCurrentFailedNodes();

        // stripe级别数据丢失检测
        int loss_stripes = checkStripeDataLoss(cur_failed, stripe_list, ecn, eck);

        if (verbose) {
            cout << "[Day " << event->timestamp << "] new_fails=" << event->fail_node_ids.size()
                 << " total_failed=" << cur_failed.size()
                 << " loss_stripes=" << loss_stripes << endl;
        }

        if (loss_stripes > 0) {
            handleDataLoss(scheduler, stripe_list, code, ecn, eck, ecw,
                           scenario, method, conf, ec, verbose, loss_stripes);
            reader.advance();
            continue;
        }

        RepairDecision decision;
        if (strategy == 0)      decision = scheduler.decideImmediate();
        else if (strategy == 1) decision = scheduler.decideLazy();
        else {
            TraceEvent* next = reader.peekNextEvent();
            vector<int> next_ids = next ? next->fail_node_ids : vector<int>{};
            decision = scheduler.decideTraceDriven(next_ids);
        }

        if (decision == REPAIR_IMMEDIATE) {
            vector<int> failed = scheduler.executeFullRepair();
            if (verbose) cout << "  -> REPAIR " << failed.size() << " nodes" << endl;
            applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler);
        } else {
            if (verbose) cout << "  -> SKIP" << endl;
        }

        reader.advance();
    }

    // 收尾修复
    if (scheduler.getCurrentFailureCount() > 0) {
        vector<int> failed = scheduler.executeFullRepair();
        applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                        scenario, method, conf, ec, scheduler);
    }

    return scheduler.getStats();
}

// =================================================================================
// Strategy 3: RiskAwarePartial
// =================================================================================
RepairStats runRiskAwarePartial(TraceReader& reader,
                                string code, int ecn, int eck, int ecw,
                                const vector<Stripe*>& stripe_list, ECBase* ec,
                                int num_agents, string scenario, int method,
                                Config* conf, bool verbose,
                                double threshold_partial = 0.5,
                                double threshold_full    = 0.8)
{
    RepairSchedulerNew scheduler(ecn, eck, ecw);
    scheduler.verbose = verbose;
    reader.reset();

    if (verbose) {
        cout << "\n======================================================" << endl;
        cout << "  Strategy: RiskAwarePartial" << endl;
        cout << "  Thresholds: partial=" << threshold_partial
             << " full=" << threshold_full << endl;
        cout << "======================================================" << endl;
    }

    while (reader.hasCurrentEvent()) {
        TraceEvent* event = reader.getCurrentEvent();
        scheduler.addFailures(event->fail_node_ids);
        int loss_stripes = checkStripeDataLoss(scheduler.getCurrentFailedNodes(), stripe_list, ecn, eck);

        if (verbose)
            cout << "[Day " << event->timestamp << "] new_fails=" << event->fail_node_ids.size()
                 << " total_failed=" << scheduler.getCurrentFailureCount()
                 << " loss_stripes=" << loss_stripes << endl;

        if (loss_stripes > 0) {
            handleDataLoss(scheduler, stripe_list, code, ecn, eck, ecw,
                           scenario, method, conf, ec, verbose, loss_stripes);
            reader.advance();
            continue;
        }

        // 收集未来多步故障窗口（最多看5步）
        vector<vector<int>> future_windows;
        TraceReader tmp_reader = reader; // 浅拷贝读取位置
        tmp_reader.advance();
        int lookahead_max = 5;
        for (int i = 0; i < lookahead_max && tmp_reader.hasCurrentEvent(); i++) {
            TraceEvent* ev = tmp_reader.getCurrentEvent();
            future_windows.push_back(ev->fail_node_ids);
            tmp_reader.advance();
        }

        RepairDecision decision = scheduler.decideRiskAwarePartial(
            future_windows, threshold_partial, threshold_full);

        if (decision == REPAIR_IMMEDIATE) {
            vector<int> failed = scheduler.executeFullRepair();
            if (verbose) cout << "  -> FULL REPAIR " << failed.size() << " nodes" << endl;
            applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler);
        } else if (decision == REPAIR_PARTIAL) {
            // 部分修复：只修复风险最高的一半节点
            vector<int> cur = scheduler.getCurrentFailedNodes();
            int repair_count = max(1, (int)cur.size() / 2);
            vector<int> to_repair(cur.begin(), cur.begin() + repair_count);
            vector<int> repaired = scheduler.executePartialRepair(to_repair);
            if (verbose) cout << "  -> PARTIAL REPAIR " << repaired.size()
                              << "/" << cur.size() << " nodes" << endl;
            applyRepairCost(repaired, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler);
        } else {
            if (verbose) cout << "  -> SKIP" << endl;
        }

        reader.advance();
    }

    if (scheduler.getCurrentFailureCount() > 0) {
        vector<int> failed = scheduler.executeFullRepair();
        applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                        scenario, method, conf, ec, scheduler);
    }

    return scheduler.getStats();
}

// =================================================================================
// Strategy 4: StripeDifferentiated
// =================================================================================
RepairStats runStripeDifferentiated(TraceReader& reader,
                                    string code, int ecn, int eck, int ecw,
                                    const vector<Stripe*>& stripe_list, ECBase* ec,
                                    int num_agents, string scenario, int method,
                                    Config* conf, bool verbose,
                                    double stripe_risk_threshold = 0.75)
{
    RepairSchedulerNew scheduler(ecn, eck, ecw);
    scheduler.verbose = verbose;
    reader.reset();

    if (verbose) {
        cout << "\n======================================================" << endl;
        cout << "  Strategy: StripeDifferentiated" << endl;
        cout << "  Stripe risk threshold: " << stripe_risk_threshold << endl;
        cout << "======================================================" << endl;
    }

    while (reader.hasCurrentEvent()) {
        TraceEvent* event = reader.getCurrentEvent();
        scheduler.addFailures(event->fail_node_ids);
        int loss_stripes = checkStripeDataLoss(scheduler.getCurrentFailedNodes(), stripe_list, ecn, eck);

        if (verbose)
            cout << "[Day " << event->timestamp << "] new_fails=" << event->fail_node_ids.size()
                 << " total_failed=" << scheduler.getCurrentFailureCount()
                 << " loss_stripes=" << loss_stripes << endl;

        if (loss_stripes > 0) {
            handleDataLoss(scheduler, stripe_list, code, ecn, eck, ecw,
                           scenario, method, conf, ec, verbose, loss_stripes);
            reader.advance();
            continue;
        }

        TraceEvent* next_event = reader.peekNextEvent();
        vector<int> next_ids = next_event ? next_event->fail_node_ids : vector<int>{};

        auto [decision, nodes_to_repair] = scheduler.decideStripeDifferentiated(
            stripe_list, next_ids, stripe_risk_threshold);

        if (decision == REPAIR_IMMEDIATE || decision == REPAIR_PARTIAL) {
            vector<int> repaired;
            if (decision == REPAIR_IMMEDIATE) {
                repaired = scheduler.executeFullRepair();
                if (verbose) cout << "  -> FULL REPAIR " << repaired.size() << " nodes" << endl;
            } else {
                repaired = scheduler.executePartialRepair(nodes_to_repair);
                if (verbose) cout << "  -> STRIPE-DRIVEN PARTIAL REPAIR "
                                  << repaired.size() << "/" 
                                  << scheduler.getCurrentFailedNodes().size() + repaired.size()
                                  << " nodes" << endl;
                // 打印哪些stripe被保护了
                if (verbose) {
                    cout << "     Protected stripes via node repair: [";
                    for (int nid : repaired) cout << nid << " ";
                    cout << "]" << endl;
                }
            }
            applyRepairCost(repaired, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler);
        } else {
            if (verbose) cout << "  -> SKIP (no critical stripes)" << endl;
        }

        reader.advance();
    }

    if (scheduler.getCurrentFailureCount() > 0) {
        vector<int> failed = scheduler.executeFullRepair();
        applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                        scenario, method, conf, ec, scheduler);
    }

    return scheduler.getStats();
}

// =================================================================================
// Strategy 5: Combined
// =================================================================================
RepairStats runCombined(TraceReader& reader,
                        string code, int ecn, int eck, int ecw,
                        const vector<Stripe*>& stripe_list, ECBase* ec,
                        int num_agents, string scenario, int method,
                        Config* conf, bool verbose,
                        double threshold_partial = 0.5,
                        double threshold_full    = 0.8,
                        double stripe_risk_thr   = 0.75)
{
    RepairSchedulerNew scheduler(ecn, eck, ecw);
    scheduler.verbose = verbose;
    reader.reset();

    if (verbose) {
        cout << "\n======================================================" << endl;
        cout << "  Strategy: Combined (RiskAware + StripeDifferentiated)" << endl;
        cout << "  risk_partial=" << threshold_partial
             << " risk_full=" << threshold_full
             << " stripe_thr=" << stripe_risk_thr << endl;
        cout << "======================================================" << endl;
    }

    while (reader.hasCurrentEvent()) {
        TraceEvent* event = reader.getCurrentEvent();
        scheduler.addFailures(event->fail_node_ids);
        int loss_stripes = checkStripeDataLoss(scheduler.getCurrentFailedNodes(), stripe_list, ecn, eck);

        if (verbose)
            cout << "[Day " << event->timestamp << "] new_fails=" << event->fail_node_ids.size()
                 << " total_failed=" << scheduler.getCurrentFailureCount()
                 << " loss_stripes=" << loss_stripes << endl;

        if (loss_stripes > 0) {
            handleDataLoss(scheduler, stripe_list, code, ecn, eck, ecw,
                           scenario, method, conf, ec, verbose, loss_stripes);
            reader.advance();
            continue;
        }

        // 收集未来多步故障窗口
        vector<vector<int>> future_windows;
        TraceReader tmp_reader = reader;
        tmp_reader.advance();
        for (int i = 0; i < 5 && tmp_reader.hasCurrentEvent(); i++) {
            future_windows.push_back(tmp_reader.getCurrentEvent()->fail_node_ids);
            tmp_reader.advance();
        }

        auto [decision, nodes_to_repair] = scheduler.decideCombined(
            stripe_list, future_windows,
            threshold_partial, threshold_full, stripe_risk_thr);

        if (decision == REPAIR_IMMEDIATE) {
            vector<int> failed = scheduler.executeFullRepair();
            if (verbose) cout << "  -> FULL REPAIR " << failed.size() << " nodes" << endl;
            applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler);
        } else if (decision == REPAIR_PARTIAL) {
            vector<int> repaired = scheduler.executePartialRepair(nodes_to_repair);
            if (verbose) cout << "  -> COMBINED PARTIAL REPAIR " << repaired.size() << " nodes"
                              << " (remaining: " << scheduler.getCurrentFailureCount() << ")" << endl;
            applyRepairCost(repaired, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler);
        } else {
            if (verbose) cout << "  -> SKIP" << endl;
        }

        reader.advance();
    }

    if (scheduler.getCurrentFailureCount() > 0) {
        vector<int> failed = scheduler.executeFullRepair();
        applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                        scenario, method, conf, ec, scheduler);
    }

    return scheduler.getStats();
}

// =================================================================================
// 打印详细对比表
// =================================================================================
void printComparisonTable(const string names[], const RepairStats stats[], int n,
                          int event_count, const string& code, int ecn, int eck)
{
    cout << "\n============================================================" << endl;
    cout << "                  COMPARISON SUMMARY" << endl;
    cout << "  Code: " << code << " (" << ecn << "," << eck << ")" << endl;
    cout << "  Trace events: " << event_count << endl;
    cout << "============================================================" << endl;

    printf("%-22s | %6s | %10s | %10s | %8s | %7s | %7s | %5s\n",
           "Strategy", "Rounds", "Load(blks)", "Bdwt(blks)",
           "DataLoss", "MaxFail", "Partial", "Skips");
    printf("%-22s-|-%6s-|-%10s-|-%10s-|-%8s-|-%7s-|-%7s-|-%5s\n",
           "----------------------", "------", "----------", "----------",
           "--------", "-------", "-------", "-----");

    for (int i = 0; i < n; i++) {
        printf("%-22s | %6d | %10.2f | %10.2f | %8d | %7d | %7d | %5d\n",
               names[i].c_str(),
               stats[i].total_repair_rounds,
               stats[i].total_repair_load,
               stats[i].total_repair_bdwt,
               stats[i].data_loss_events,
               stats[i].max_concurrent_failures,
               stats[i].partial_repair_count,
               stats[i].skipped_repair_count);
    }
    cout << "============================================================" << endl;

    // 相比 Immediate 的节省
    if (stats[0].total_repair_load > 0) {
        cout << "\nLoad savings vs Immediate:" << endl;
        for (int i = 1; i < n; i++) {
            double save = (1.0 - stats[i].total_repair_load / stats[0].total_repair_load) * 100;
            printf("  %-22s: %+.1f%% (data_loss=%d, partial_repairs=%d)\n",
                   names[i].c_str(), save,
                   stats[i].data_loss_events,
                   stats[i].partial_repair_count);
        }
    }

    // 相比 Lazy 的安全性提升
    cout << "\nSafety improvement vs Lazy:" << endl;
    for (int i = 2; i < n; i++) {
        int loss_reduction = stats[1].data_loss_events - stats[i].data_loss_events;
        printf("  %-22s: data_loss reduced by %d events\n",
               names[i].c_str(), loss_reduction);
    }
}

// =================================================================================
// main
// =================================================================================
int main(int argc, char** argv) {
    if (argc < 10) {
        cout << "Usage: ./RepairSimulator_new code ecn eck ecw num_agents "
             << "trace_file method scenario placement_file" << endl;
        return 0;
    }

    string code          = argv[1];
    int    ecn           = atoi(argv[2]);
    int    eck           = atoi(argv[3]);
    int    ecw           = atoi(argv[4]);
    int    num_agents    = atoi(argv[5]);
    string trace_file    = argv[6];
    int    method        = atoi(argv[7]);
    string scenario      = argv[8];
    string placement_file = argv[9];

    // 可选参数：控制新策略的阈值
    double threshold_partial = (argc > 10) ? atof(argv[10]) : 0.5;
    double threshold_full    = (argc > 11) ? atof(argv[11]) : 0.8;
    double stripe_risk_thr   = (argc > 12) ? atof(argv[12]) : 0.75;

    cout << "============================================" << endl;
    cout << "  Extended Repair Strategy Simulator" << endl;
    cout << "============================================" << endl;
    cout << "Code: " << code << " (" << ecn << "," << eck << ") ecw=" << ecw << endl;
    cout << "Nodes: " << num_agents << " | Fault tolerance: " << (ecn - eck) << endl;
    cout << "Trace: " << trace_file << endl;
    cout << "Scenario: " << scenario << " | Method: " << method << endl;
    cout << "RiskAware thresholds: partial=" << threshold_partial
         << " full=" << threshold_full << endl;
    cout << "Stripe risk threshold: " << stripe_risk_thr << endl;

    // 初始化
    string conf_path = "conf/sysSetting.xml";
    Config* conf = new Config(conf_path);
    conf->_agents_num = num_agents;

    TraceReader reader;
    if (!reader.loadTrace(trace_file)) {
        cerr << "Failed to load trace: " << trace_file << endl;
        return 1;
    }

    vector<Stripe*> stripe_list = loadPlacement(placement_file);
    if (stripe_list.empty()) {
        cerr << "No stripes loaded from " << placement_file << endl;
        return 1;
    }
    cout << "Loaded " << stripe_list.size() << " stripes" << endl;

    // 实例化EC
    vector<string> param;
    ECBase* ec;
    if      (code == "Clay")      ec = new Clay(ecn, eck, ecw, {to_string(ecn - 1)});
    else if (code == "RDP")       ec = new RDP(ecn, eck, ecw, param);
    else if (code == "HHXORPlus") ec = new HHXORPlus(ecn, eck, ecw, param);
    else if (code == "BUTTERFLY") ec = new BUTTERFLY(ecn, eck, ecw, param);
    else if (code == "RSCONV")    ec = new RSCONV(ecn, eck, ecw, param);
    else if (code == "RSPIPE")    ec = new RSPIPE(ecn, eck, ecw, param);
    else { cerr << "Unknown code: " << code << endl; return 1; }

    // ============================================================
    // 运行全部6种策略
    // ============================================================
    const int NUM_STRATEGIES = 6;
    string names[NUM_STRATEGIES] = {
        "Immediate",
        "Lazy",
        "TraceDriven",
        "RiskAwarePartial",
        "StripeDiff",
        "Combined"
    };
    RepairStats stats[NUM_STRATEGIES];

    bool verbose = true; // 设为true输出详细日志

    cout << "\nRunning strategies..." << endl;
    cout << "  [4/6] RiskAwarePartial (thr_p=" << threshold_partial
         << " thr_f=" << threshold_full << ")..." << endl;
    stats[3] = runRiskAwarePartial(reader, code, ecn, eck, ecw,
                                    stripe_list, ec, num_agents, scenario,
                                    method, conf, verbose,
                                    threshold_partial, threshold_full);

    cout << "  [5/6] StripeDifferentiated (stripe_thr=" << stripe_risk_thr << ")..." << endl;
    stats[4] = runStripeDifferentiated(reader, code, ecn, eck, ecw,
                                        stripe_list, ec, num_agents, scenario,
                                        method, conf, verbose,
                                        stripe_risk_thr);

    cout << "  [6/6] Combined..." << endl;
    stats[5] = runCombined(reader, code, ecn, eck, ecw,
                           stripe_list, ec, num_agents, scenario,
                           method, conf, verbose,
                           threshold_partial, threshold_full, stripe_risk_thr);


    cout << "  [1/6] Immediate..." << endl;
    stats[0] = runOriginalStrategy(0, reader, code, ecn, eck, ecw,
                                    stripe_list, ec, num_agents, scenario,
                                    method, conf, verbose);

    cout << "  [2/6] Lazy..." << endl;
    stats[1] = runOriginalStrategy(1, reader, code, ecn, eck, ecw,
                                    stripe_list, ec, num_agents, scenario,
                                    method, conf, verbose);

    cout << "  [3/6] TraceDriven..." << endl;
    stats[2] = runOriginalStrategy(2, reader, code, ecn, eck, ecw,
                                    stripe_list, ec, num_agents, scenario,
                                    method, conf, verbose);

    

    // ============================================================
    // 输出对比表
    // ============================================================
    printComparisonTable(names, stats, NUM_STRATEGIES,
                         reader.getEventCount(), code, ecn, eck);

    // ============================================================
    // Pareto分析：Load Saving vs Data Loss
    // ============================================================
    cout << "\n--- Pareto Analysis (Load Saving vs Safety) ---" << endl;
    printf("%-22s | %10s | %8s | %8s | %s\n",
           "Strategy", "LoadSaving%", "DataLoss", "Partial", "Note");
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        double save = (stats[0].total_repair_load > 0)
            ? (1.0 - stats[i].total_repair_load / stats[0].total_repair_load) * 100
            : 0.0;
        string note = "";
        if (stats[i].data_loss_events == 0)           note = "✓ Safe";
        else if (stats[i].data_loss_events <= 2)      note = "~ Low risk";
        else                                           note = "✗ Risky";
        printf("%-22s | %10.1f | %8d | %8d | %s\n",
               names[i].c_str(), save,
               stats[i].data_loss_events,
               stats[i].partial_repair_count,
               note.c_str());
    }

    if (conf) delete conf;
    return 0;
}
