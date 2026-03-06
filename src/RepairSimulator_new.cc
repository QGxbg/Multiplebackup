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
#include "RepairScheduler_new.hh"
#include "MLPredictor.hh"

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
// ECDAG 不再直接使用，但 ParallelSolution 内部依赖，保留间接引用即可

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

// =================================================================================
// 批处理修复代价计算（复用 Sim_parallel 的完整流程）
//   1. 筛选受影响的stripe子集
//   2. ParallelSolution::genRepairBatches 生成批处理修复方案
//   3. 从 RepairBatch 统计 load/bdwt 累加到 scheduler
// =================================================================================
void applyRepairCost(const vector<int>& repaired_nodes,
                     const vector<Stripe*>& stripe_list,
                     string code, int ecn, int eck, int ecw,
                     string scenario, int method, Config* conf, ECBase* ec,
                     RepairSchedulerNew& scheduler,
                     bool verbose = false)
{
    if (repaired_nodes.empty()) return;

    int fail_num     = (int)repaired_nodes.size();
    int num_agents   = conf->_agents_num;
    int standby_size = fail_num;

    // Step 1: 筛选包含故障节点且未超出容错的stripe
    vector<Stripe*> affected_stripes;
    for (Stripe* s : stripe_list) {
        const vector<int>& placement = s->getPlacement();
        int hits = 0;
        for (int nid : repaired_nodes)
            if (find(placement.begin(), placement.end(), nid) != placement.end())
                hits++;
        if (hits > 0 && hits <= (ecn - eck))
            affected_stripes.push_back(s);
    }
    if (affected_stripes.empty()) return;

    if (verbose) {
        cout << "  [Repair] nodes: ";
        for (int nid : repaired_nodes) cout << "Node-" << nid << " ";
        cout << "\n  [Repair] affected stripes: " << affected_stripes.size() << " stripes:" << endl;
        for (Stripe* s : affected_stripes) {
            const vector<int>& pl = s->getPlacement();
            // 列出该条带中哪些节点在被修复
            vector<int> stripe_failed;
            for (int nid : repaired_nodes)
                if (find(pl.begin(), pl.end(), nid) != pl.end())
                    stripe_failed.push_back(nid);
            printf("    Stripe-%-4d: repaired nodes [", s->getStripeId());
            for (int i = 0; i < (int)stripe_failed.size(); i++) {
                if (i) cout << ",";
                cout << stripe_failed[i];
            }
            printf("]  (n=%d tol=%d)\n",
                   ecn, ecn - eck);
        }
    }

    // Step 2: 初始化 ParallelSolution，batchsize=1 保持精确统计
    ParallelSolution* sol = new ParallelSolution(1, standby_size, num_agents, method);
    sol->init(affected_stripes, ec, code, conf);

    // Step 3: 生成修复批次（enqueue=false，结果存入 _batch_list）
    sol->genRepairBatches(fail_num, repaired_nodes, num_agents, scenario, false);

    // Step 4: 统计所有批次的 load/bdwt
    vector<RepairBatch*> batches = sol->getRepairBatches();
    int total_load = 0, total_bdwt = 0;
    for (RepairBatch* batch : batches) {
        total_load += batch->getLoad();
        total_bdwt += batch->getBdwt();
    }

    if (verbose) {
        printf("  [Cost] batches=%d load=%.2f blks bdwt=%.2f blks\n",
               (int)batches.size(),
               1.0 * total_load / ecw,
               1.0 * total_bdwt / ecw);
    }

    scheduler.recordRepairLoad(1.0 * total_load / ecw,
                               1.0 * total_bdwt / ecw);
    delete sol;
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
                    bool verbose, int loss_stripes)
{
    // 只记录丢失事件和条带数，不修复，坏节点继续留在 _current_failed
    // 后续这些节点涉及的其他条带若变危险，再触发正常修复流程
    scheduler.recordDataLoss(loss_stripes);
    if (verbose) {
        printf("  *** DATA LOSS! %d stripes lost (failed nodes remain, will repair when other stripes at risk) ***\n",
               loss_stripes);
    }
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
            handleDataLoss(scheduler, verbose, loss_stripes);
            // nodes remain in _current_failed, continue to normal decision
        }

        if (strategy == 0) {
            // Immediate：有故障就修
            RepairDecision decision = scheduler.decideImmediate();
            if (decision == REPAIR_IMMEDIATE) {
                vector<int> failed = scheduler.executeFullRepair();
                if (verbose)
                    printf("  -> REPAIR %d nodes  [Reason: Immediate]\n", (int)failed.size());
                applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                                scenario, method, conf, ec, scheduler);
            }
        } else if (strategy == 1) {
            // Lazy：条带级，某条带故障数 >= tolerance 才修
            auto result_lazy = scheduler.decideLazy(stripe_list);
            RepairDecision decision = result_lazy.first;
            vector<int> nodes_to_repair = result_lazy.second;
            if (decision == REPAIR_IMMEDIATE) {
                vector<int> repaired = scheduler.executePartialRepair(nodes_to_repair);
                if (verbose)
                    printf("  -> REPAIR %d nodes  [Reason: Lazy stripe-level, some stripe cur >= tolerance=%d]\n",
                           (int)repaired.size(), ecn - eck);
                applyRepairCost(repaired, stripe_list, code, ecn, eck, ecw,
                                scenario, method, conf, ec, scheduler);
            } else {
                if (verbose)
                    printf("  -> SKIP  [Lazy: no stripe has cur >= tolerance=%d]\n", ecn - eck);
            }
        } else {
            // TraceDriven：条带级决策
            TraceEvent* next = reader.peekNextEvent();
            vector<int> next_ids = next ? next->fail_node_ids : vector<int>{};
            auto result_td = scheduler.decideTraceDrivenStripe(stripe_list, next_ids);
            RepairDecision decision = result_td.first;
            vector<int> nodes_to_repair = result_td.second;

            if (decision == REPAIR_IMMEDIATE || decision == REPAIR_PARTIAL) {
                vector<int> repaired = scheduler.executePartialRepair(nodes_to_repair);
                if (verbose) {
                    printf("  -> REPAIR %d/%d nodes  [Reason: TraceDriven stripe-level, "
                           "dangerous stripes need repair]\n",
                           (int)repaired.size(), scheduler.getCurrentFailureCount() + (int)repaired.size());
                    cout << "     Repaired nodes: ";
                    for (int nid : repaired) cout << "Node-" << nid << " ";
                    cout << endl;
                }
                applyRepairCost(repaired, stripe_list, code, ecn, eck, ecw,
                                scenario, method, conf, ec, scheduler);
            } else {
                if (verbose)
                    printf("  -> SKIP  [No dangerous stripe: all stripes safe with cur+next <= tolerance=%d]\n",
                           ecn - eck);
            }
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
            handleDataLoss(scheduler, verbose, loss_stripes);
            // nodes remain in _current_failed, continue to normal decision
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
            handleDataLoss(scheduler, verbose, loss_stripes);
            // nodes remain in _current_failed, continue to normal decision
        }

        TraceEvent* next_event = reader.peekNextEvent();
        vector<int> next_ids = next_event ? next_event->fail_node_ids : vector<int>{};

        auto result_sd = scheduler.decideStripeDifferentiated(
            stripe_list, next_ids, stripe_risk_threshold);
        RepairDecision decision = result_sd.first;
        vector<int> nodes_to_repair = result_sd.second;

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
            handleDataLoss(scheduler, verbose, loss_stripes);
            // nodes remain in _current_failed, continue to normal decision
        }

        // 收集未来多步故障窗口
        vector<vector<int>> future_windows;
        TraceReader tmp_reader = reader;
        tmp_reader.advance();
        for (int i = 0; i < 5 && tmp_reader.hasCurrentEvent(); i++) {
            future_windows.push_back(tmp_reader.getCurrentEvent()->fail_node_ids);
            tmp_reader.advance();
        }

        auto result_cb = scheduler.decideCombined(
            stripe_list, future_windows,
            threshold_partial, threshold_full, stripe_risk_thr);
        RepairDecision decision = result_cb.first;
        vector<int> nodes_to_repair = result_cb.second;

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
// Strategy 6: MLDriven
//   决策逻辑与 TraceDriven 完全一致：cur + pred > tolerance 才修
//   唯一区别：next_ids 来自 MLPredictor（有误报/漏报），而非真实 trace
// =================================================================================
RepairStats runMLDriven(TraceReader& reader,
                        string code, int ecn, int eck, int ecw,
                        const vector<Stripe*>& stripe_list, ECBase* ec,
                        int num_agents, string scenario, int method,
                        Config* conf, bool verbose,
                        double fpr, double fnr,
                        int seed = 42)
{
    RepairSchedulerNew scheduler(ecn, eck, ecw);
    reader.reset();

    // 初始化 ML 预测器
    MLPredictor predictor(num_agents, fpr, fnr, seed);

    if (verbose) {
        cout << "\n======================================================" << endl;
        cout << "  Strategy: MLDriven" << endl;
        printf("  FPR=%.4f  FNR=%.4f\n", fpr, fnr);
        cout << "======================================================" << endl;
    }

    while (reader.hasCurrentEvent()) {
        TraceEvent* event = reader.getCurrentEvent();
        scheduler.addFailures(event->fail_node_ids);
        int loss_stripes = checkStripeDataLoss(
            scheduler.getCurrentFailedNodes(), stripe_list, ecn, eck);

        if (verbose)
            cout << "[Day " << event->timestamp << "] new_fails=" << event->fail_node_ids.size()
                 << " total_failed=" << scheduler.getCurrentFailureCount()
                 << " loss_stripes=" << loss_stripes << endl;

        if (loss_stripes > 0) {
            handleDataLoss(scheduler, verbose, loss_stripes);
            // nodes remain in _current_failed, continue to normal decision
        }

        // ML预测下一步故障（仅用于决策，注入误报/漏报）
        TraceEvent* next_real = reader.peekNextEvent();
        vector<int> pred_ids  = predictor.predictNextFailures(
            next_real, scheduler.getCurrentFailedNodes());

        if (verbose) {
            // 打印预测 vs 真实对比
            vector<int> real_next = next_real ? next_real->fail_node_ids : vector<int>{};
            unordered_set<int> real_set(real_next.begin(), real_next.end());
            unordered_set<int> pred_set(pred_ids.begin(), pred_ids.end());
            vector<int> tp, fp, fn;
            for (int nid : pred_ids)
                (real_set.count(nid) ? tp : fp).push_back(nid);
            for (int nid : real_next)
                if (!pred_set.count(nid)) fn.push_back(nid);
            printf("+-- [Day %d] ML Prediction: real_next=%d pred=%d  TP=%d FP=%d FN=%d\n",
                   event->timestamp, (int)real_next.size(), (int)pred_ids.size(),
                   (int)tp.size(), (int)fp.size(), (int)fn.size());
            if (!fp.empty()) {
                cout << "|  FP nodes: ";
                for (int nid : fp) cout << "Node-" << nid << " ";
                cout << " <- false alarm, may trigger unnecessary repair" << endl;
            }
            if (!fn.empty()) {
                cout << "|  FN nodes: ";
                for (int nid : fn) cout << "Node-" << nid << " ";
                cout << " <- missed, repair may be delayed" << endl;
            }
        }

        // 条带级决策：与 TraceDriven 完全一致，只是 next_ids 来自 ML 预测
        auto result_ml = scheduler.decideTraceDrivenStripe(stripe_list, pred_ids);
        RepairDecision decision = result_ml.first;
        vector<int> nodes_to_repair = result_ml.second;

        if (decision == REPAIR_IMMEDIATE || decision == REPAIR_PARTIAL) {
            // 修复的是真实故障节点（nodes_to_repair 来自 _current_failed）
            vector<int> repaired = scheduler.executePartialRepair(nodes_to_repair);
            if (verbose) {
                printf("  -> REPAIR %d/%d nodes  [Reason: ML stripe-level, "
                       "dangerous stripes detected with pred_ids]\n",
                       (int)repaired.size(),
                       scheduler.getCurrentFailureCount() + (int)repaired.size());
                cout << "     Repaired nodes: ";
                for (int nid : repaired) cout << "Node-" << nid << " ";
                cout << endl;
            }
            applyRepairCost(repaired, stripe_list, code, ecn, eck, ecw,
                            scenario, method, conf, ec, scheduler, verbose);
        } else {
            if (verbose)
                printf("  -> SKIP  [No dangerous stripe with pred_ids, safe to wait]\n");
        }

        reader.advance();
    }

    if (scheduler.getCurrentFailureCount() > 0) {
        vector<int> failed = scheduler.executeFullRepair();
        applyRepairCost(failed, stripe_list, code, ecn, eck, ecw,
                        scenario, method, conf, ec, scheduler, verbose);
    }

    if (verbose) predictor.printStats();

    // 把预测器统计挂到 stats 的 avg_risk 字段（复用字段存 actual FPR）
    RepairStats stats = scheduler.getStats();
    stats.avg_risk_at_repair = predictor.getStats().fpr();
    return stats;
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

    printf("%-22s | %6s | %10s | %10s | %8s | %10s | %7s | %5s\n",
           "Strategy", "Rounds", "Load(blks)", "Bdwt(blks)",
           "LossEvents", "LostStripes", "MaxFail", "Skips");
    printf("%-22s-|-%6s-|-%10s-|-%10s-|-%8s-|-%10s-|-%7s-|-%5s\n",
           "----------------------", "------", "----------", "----------",
           "----------", "-----------", "-------", "-----");

    for (int i = 0; i < n; i++) {
        printf("%-22s | %6d | %10.2f | %10.2f | %8d | %10d | %7d | %5d\n",
               names[i].c_str(),
               stats[i].total_repair_rounds,
               stats[i].total_repair_load,
               stats[i].total_repair_bdwt,
               stats[i].data_loss_events,
               stats[i].lost_stripes_count,
               stats[i].max_concurrent_failures,
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
        cout << "Usage: ./RepairSimulator_new" << endl;
        cout << "  Required:" << endl;
        cout << "    1. code           (e.g. Clay)" << endl;
        cout << "    2. ecn            (e.g. 14)" << endl;
        cout << "    3. eck            (e.g. 10)" << endl;
        cout << "    4. ecw            (e.g. 4)" << endl;
        cout << "    5. num_agents     (e.g. 40)" << endl;
        cout << "    6. trace_file     (e.g. trace.txt)" << endl;
        cout << "    7. method         (e.g. 1)" << endl;
        cout << "    8. scenario       (e.g. standby)" << endl;
        cout << "    9. placement_file (e.g. placement.txt)" << endl;
        cout << "  Optional:" << endl;
        cout << "   10. threshold_partial  (default 0.5,  unused by ML strategies)" << endl;
        cout << "   11. threshold_full     (default 0.8,  unused by ML strategies)" << endl;
        cout << "   12. stripe_risk_thr    (default 0.75, unused by ML strategies)" << endl;
        cout << "   13. ml_fpr_low         (default 0.002, ML-Low false positive rate)" << endl;
        cout << "   14. ml_fpr_high        (default 0.025, ML-High false positive rate)" << endl;
        cout << "   15. ml_fnr             (default 0.05,  both ML strategies false negative rate)" << endl;
        cout << endl;
        cout << "Examples:" << endl;
        cout << "  # Default ML params:" << endl;
        cout << "  ./RepairSimulator_new Clay 14 10 4 40 trace.txt 1 standby placement.txt" << endl;
        cout << "  # FNR sensitivity scan (fix FPR, vary FNR):" << endl;
        cout << "      ./RepairSimulator_new Clay 14 10 4 40 trace.txt 1 standby placement.txt" << endl;
        cout << "          0.5 0.8 0.75 0.002 0.025 0.10" << endl;
        cout << "  # FPR sensitivity scan (fix FNR, vary FPR):" << endl;
        cout << "      ./RepairSimulator_new Clay 14 10 4 40 trace.txt 1 standby placement.txt" << endl;
        cout << "          0.5 0.8 0.75 0.010 0.010 0.05" << endl;
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

    // 可选参数（argv[10]~[15]）：
    //   [10] threshold_partial  风险分阈值（部分修复）  默认 0.5  （仅旧策略使用，ML不用）
    //   [11] threshold_full     风险分阈值（全量修复）  默认 0.8  （仅旧策略使用，ML不用）
    //   [12] stripe_risk_thr    条带风险阈值            默认 0.75 （仅旧策略使用，ML不用）
    //   [13] ml_fpr_low         ML-Low 误报率           默认 0.002 (0.2%)
    //   [14] ml_fpr_high        ML-High 误报率          默认 0.025 (2.5%)
    //   [15] ml_fnr             两种ML策略共用漏报率    默认 0.05  (5%)
    //
    // 实验用法示例：
    //   FNR敏感性扫描（固定FPR，改变FNR）：
    //     ./RepairSimulator_new Clay 14 10 4 40 trace.txt 1 standby placement.txt     //         0.5 0.8 0.75 0.002 0.025 <fnr>
    //
    //   FPR敏感性扫描（固定FNR，改变FPR）：
    //     ./RepairSimulator_new Clay 14 10 4 40 trace.txt 1 standby placement.txt     //         0.5 0.8 0.75 <fpr_low> <fpr_high> 0.05
    double threshold_partial = (argc > 10) ? atof(argv[10]) : 0.5;
    double threshold_full    = (argc > 11) ? atof(argv[11]) : 0.8;
    double stripe_risk_thr   = (argc > 12) ? atof(argv[12]) : 0.75;
    double ml_fpr_low        = (argc > 13) ? atof(argv[13]) : 0.002;
    double ml_fpr_high       = (argc > 14) ? atof(argv[14]) : 0.025;
    double ml_fnr            = (argc > 15) ? atof(argv[15]) : 0.05;

    cout << "============================================" << endl;
    cout << "  Extended Repair Strategy Simulator" << endl;
    cout << "============================================" << endl;
    cout << "Code: " << code << " (" << ecn << "," << eck << ") ecw=" << ecw << endl;
    cout << "Nodes: " << num_agents << " | Fault tolerance: " << (ecn - eck) << endl;
    cout << "Trace: " << trace_file << endl;
    cout << "Scenario: " << scenario << " | Method: " << method << endl;
    printf("ML params: fpr_low=%.4f  fpr_high=%.4f  fnr=%.4f\n",
           ml_fpr_low, ml_fpr_high, ml_fnr);

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
    // 运行5种策略
    // ============================================================
    const int NUM_STRATEGIES = 5;
    string names[NUM_STRATEGIES] = {
        "Immediate",
        "Lazy",
        "Perfect Prediction",  // FPR=0 FNR=0，完美预测上界
        "ML-Low FPR",          // FPR=0.2%  FNR=5%
        "ML-High FPR",         // FPR=2.5%  FNR=5%
    };
    RepairStats stats[NUM_STRATEGIES];

    bool verbose = false; // 设为true输出每步详细日志

    cout << "\nRunning strategies..." << endl;

    cout << "  [1/5] Immediate..." << endl;
    stats[0] = runOriginalStrategy(0, reader, code, ecn, eck, ecw,
                                   stripe_list, ec, num_agents, scenario,
                                   method, conf, verbose);

    cout << "  [2/5] Lazy..." << endl;
    stats[1] = runOriginalStrategy(1, reader, code, ecn, eck, ecw,
                                   stripe_list, ec, num_agents, scenario,
                                   method, conf, verbose);

    cout << "  [3/5] Perfect Prediction (TraceDriven)..." << endl;
    stats[2] = runOriginalStrategy(2, reader, code, ecn, eck, ecw,
                                   stripe_list, ec, num_agents, scenario,
                                   method, conf, verbose);

    cout << "  [4/5] ML-Low FPR (fpr=" << ml_fpr_low
         << " fnr=" << ml_fnr << ")..." << endl;
    stats[3] = runMLDriven(reader, code, ecn, eck, ecw,
                           stripe_list, ec, num_agents, scenario,
                           method, conf, verbose,
                           ml_fpr_low, ml_fnr);

    cout << "  [5/5] ML-High FPR (fpr=" << ml_fpr_high
         << " fnr=" << ml_fnr << ")..." << endl;
    stats[4] = runMLDriven(reader, code, ecn, eck, ecw,
                           stripe_list, ec, num_agents, scenario,
                           method, conf, verbose,
                           ml_fpr_high, ml_fnr, 123);

    // ============================================================
    // 输出对比表
    // ============================================================
    printComparisonTable(names, stats, NUM_STRATEGIES,
                         reader.getEventCount(), code, ecn, eck);

    // ============================================================
    // Pareto分析：Load Saving vs Data Loss
    // ============================================================
    cout << "\n--- Pareto Analysis (Load Saving vs Safety) ---" << endl;
    printf("%-22s | %10s | %8s | %8s | %8s | %s\n",
           "Strategy", "LoadSaving%", "DataLoss", "Partial", "ActualFPR", "Note");
    for (int i = 0; i < NUM_STRATEGIES; i++) {
        double save = (stats[0].total_repair_load > 0)
            ? (1.0 - stats[i].total_repair_load / stats[0].total_repair_load) * 100
            : 0.0;
        string note = "";
        if (stats[i].data_loss_events == 0)           note = "Safe";
        else if (stats[i].data_loss_events <= 2)      note = "Low risk";
        else                                           note = "Risky";
        printf("%-22s | %10.1f | %8d | %8d | %8.4f | %s\n",
               names[i].c_str(), save,
               stats[i].data_loss_events,
               stats[i].partial_repair_count,
               stats[i].avg_risk_at_repair,   // ML策略复用此字段存actual FPR
               note.c_str());
    }

    if (conf) delete conf;
    return 0;
}