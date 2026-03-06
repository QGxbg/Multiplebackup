#pragma once
// RepairScheduler_new.hh
// 扩展版调度器，依赖 common/RepairScheduler.hh 中已定义的：
//   - enum RepairDecision (含 REPAIR_PARTIAL)
//   - struct RepairStats  (含 partial_repair_count 等新字段)
// 本文件只新增：StripeRisk, NodeRepairPriority, RepairSchedulerNew

#include "common/RepairScheduler.hh"
#include "common/Stripe.hh"

#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_set>
#include <iostream>
#include <string>

using namespace std;

// =====================================================================
// 新增数据结构（原文件中没有）
// =====================================================================

struct StripeRisk {
    int    stripe_id;
    int    fail_count;
    int    tolerance;
    double risk_score;         // [0,1]
    bool   critical;           // fail_count >= tolerance
    vector<int> failed_nodes;
};

struct NodeRepairPriority {
    int    node_id;
    double priority_score;
    int    critical_stripes;
    int    high_risk_stripes;
};

// =====================================================================
// 辅助函数
// =====================================================================

inline int adaptiveLookahead(int current_failures, int tolerance) {
    if (tolerance <= 0) return 1;
    double ratio = (double)current_failures / tolerance;
    if (ratio >= 0.75) return 5;
    if (ratio >= 0.50) return 3;
    if (ratio >= 0.25) return 2;
    return 1;
}

inline double computeFailureTrend(const vector<int>& recent_daily_fails, int window = 3) {
    if (recent_daily_fails.empty()) return 0.0;
    int n = min((int)recent_daily_fails.size(), window);
    double sum = 0;
    for (int i = (int)recent_daily_fails.size() - n; i < (int)recent_daily_fails.size(); i++)
        sum += recent_daily_fails[i];
    return sum / n;
}

// =====================================================================
// RepairSchedulerNew
// =====================================================================
class RepairSchedulerNew {
public:
    int ecn, eck, ecw;
    int tolerance;

    vector<int>  _current_failed;
    RepairStats  _stats;
    vector<int>  _daily_fail_counts;
    double       _risk_sum_at_repair  = 0.0;
    int          _repair_trigger_count = 0;
    bool         verbose = false;

    RepairSchedulerNew(int n, int k, int w)
        : ecn(n), eck(k), ecw(w), tolerance(n - k)
    {
        _stats = RepairStats{};
    }

    // ------------------------------------------------------------------
    // 基础操作
    // ------------------------------------------------------------------
    bool addFailures(const vector<int>& new_fails) {
        for (int id : new_fails) {
            if (find(_current_failed.begin(), _current_failed.end(), id) == _current_failed.end())
                _current_failed.push_back(id);
        }
        _daily_fail_counts.push_back((int)new_fails.size());

        int cur = (int)_current_failed.size();
        if (cur > _stats.max_concurrent_failures)
            _stats.max_concurrent_failures = cur;

        return (cur > tolerance);
    }

    vector<int> getCurrentFailedNodes() const { return _current_failed; }
    int getCurrentFailureCount() const { return (int)_current_failed.size(); }
    void recordDataLoss(int lost_stripes = 1) {
        _stats.data_loss_events++;
        _stats.lost_stripes_count += lost_stripes;
    }
    void recordRepairLoad(double load, double bdwt) {
        _stats.total_repair_load += load;
        _stats.total_repair_bdwt += bdwt;
    }

    vector<int> executeFullRepair() {
        vector<int> repaired = _current_failed;
        _current_failed.clear();
        _stats.total_repair_rounds++;
        return repaired;
    }

    vector<int> executePartialRepair(const vector<int>& nodes_to_repair) {
        vector<int> repaired;
        for (int nid : nodes_to_repair) {
            auto it = find(_current_failed.begin(), _current_failed.end(), nid);
            if (it != _current_failed.end()) {
                repaired.push_back(nid);
                _current_failed.erase(it);
            }
        }
        if (!repaired.empty()) {
            _stats.total_repair_rounds++;
            _stats.partial_repair_count++;
        }
        return repaired;
    }

    RepairStats getStats() {
        if (_repair_trigger_count > 0)
            _stats.avg_risk_at_repair = _risk_sum_at_repair / _repair_trigger_count;
        return _stats;
    }

    void reset() {
        _current_failed.clear();
        _stats = RepairStats{};
        _daily_fail_counts.clear();
        _risk_sum_at_repair  = 0.0;
        _repair_trigger_count = 0;
    }

    // ------------------------------------------------------------------
    // Strategy 0: Immediate
    // ------------------------------------------------------------------
    RepairDecision decideImmediate() {
        if (_current_failed.empty()) return REPAIR_LAZY;
        return REPAIR_IMMEDIATE;
    }

    // ------------------------------------------------------------------
    // Strategy 1: Lazy
    // ------------------------------------------------------------------
    // Lazy Repair：条带级，某条带故障数 >= tolerance 才触发修复
    // 返回需要修复的节点（危险条带中的故障节点）
    pair<RepairDecision, vector<int>> decideLazy(const vector<Stripe*>& stripe_list)
    {
        if (_current_failed.empty()) return {REPAIR_LAZY, {}};

        unordered_map<int, Stripe*> sid_map;
        for (Stripe* s : stripe_list)
            sid_map[s->getStripeId()] = s;

        unordered_set<int> cur_set(_current_failed.begin(), _current_failed.end());
        unordered_set<int> nodes_to_repair_set;

        for (auto& kv : sid_map) {
            Stripe* s = kv.second;
            const vector<int>& pl = s->getPlacement();
            vector<int> cur_in_stripe;
            for (int nid : pl)
                if (cur_set.count(nid)) cur_in_stripe.push_back(nid);

            // 条带级触发：当前故障数 >= tolerance
            if ((int)cur_in_stripe.size() >= tolerance) {
                if (verbose)
                    printf("    [Lazy] Stripe-%d TRIGGER: cur=%d >= tolerance=%d\n",
                           s->getStripeId(), (int)cur_in_stripe.size(), tolerance);
                for (int nid : cur_in_stripe)
                    nodes_to_repair_set.insert(nid);
            }
        }

        if (nodes_to_repair_set.empty()) return {REPAIR_LAZY, {}};

        vector<int> nodes_to_repair(nodes_to_repair_set.begin(), nodes_to_repair_set.end());
        sort(nodes_to_repair.begin(), nodes_to_repair.end());
        return {REPAIR_IMMEDIATE, nodes_to_repair};
    }

    // ------------------------------------------------------------------
    // Strategy 2: TraceDriven（条带级决策）
    //
    // 对每个条带：当前故障数 + 预测下一步落在该条带的故障数 > 容错
    //             → 该条带危险，需要修复
    // 返回：(决策, 需要修复的节点列表)
    //   节点列表 = 所有危险条带中当前已故障的节点（去重）
    // ------------------------------------------------------------------
    pair<RepairDecision, vector<int>> decideTraceDrivenStripe(
            const vector<Stripe*>& stripe_list,
            const vector<int>& next_fail_ids)
    {
        if (_current_failed.empty()) return {REPAIR_LAZY, {}};

        // 构建 stripe_id -> Stripe* 映射
        unordered_map<int, Stripe*> sid_map;
        for (Stripe* s : stripe_list)
            sid_map[s->getStripeId()] = s;

        unordered_set<int> next_set(next_fail_ids.begin(), next_fail_ids.end());
        unordered_set<int> cur_set(_current_failed.begin(), _current_failed.end());

        // 找出所有危险条带，收集其中需要修复的节点
        unordered_set<int> nodes_to_repair_set;

        for (auto& [sid, s] : sid_map) {
            const vector<int>& pl = s->getPlacement();

            // 当前在该条带中故障的节点
            vector<int> cur_in_stripe, next_in_stripe;
            for (int nid : pl) {
                if (cur_set.count(nid))  cur_in_stripe.push_back(nid);
                if (next_set.count(nid)) next_in_stripe.push_back(nid);
            }

            if (cur_in_stripe.empty()) continue;

            // 条带级判断：当前故障 + 预测故障 > 容错 → 危险
            if ((int)cur_in_stripe.size() + (int)next_in_stripe.size() > tolerance) {
                if (verbose)
                    printf("    [TraceDriven] Stripe-%d DANGER: cur=%d + next=%d > tol=%d\n",
                           sid, (int)cur_in_stripe.size(),
                           (int)next_in_stripe.size(), tolerance);
                for (int nid : cur_in_stripe)
                    nodes_to_repair_set.insert(nid);
            }
        }

        if (nodes_to_repair_set.empty()) return {REPAIR_LAZY, {}};

        vector<int> nodes_to_repair(nodes_to_repair_set.begin(), nodes_to_repair_set.end());
        sort(nodes_to_repair.begin(), nodes_to_repair.end());

        // 如果需要修复的节点 == 所有故障节点，算作全量修复
        if (nodes_to_repair.size() == _current_failed.size())
            return {REPAIR_IMMEDIATE, nodes_to_repair};
        return {REPAIR_PARTIAL, nodes_to_repair};
    }

    // 保留旧接口兼容性
    RepairDecision decideTraceDriven(const vector<int>& next_fail_ids) {
        int cur  = (int)_current_failed.size();
        int next = (int)next_fail_ids.size();
        if (cur == 0 && next == 0) return REPAIR_LAZY;
        if (cur + next > tolerance) return REPAIR_IMMEDIATE;
        return REPAIR_LAZY;
    }

    // ------------------------------------------------------------------
    // Strategy 3: RiskAwarePartial
    // ------------------------------------------------------------------
    RepairDecision decideRiskAwarePartial(
            const vector<vector<int>>& future_fail_windows,
            double risk_threshold_partial = 0.5,
            double risk_threshold_full    = 0.8)
    {
        int cur = (int)_current_failed.size();
        if (cur == 0) return REPAIR_LAZY;

        int    lookahead = adaptiveLookahead(cur, tolerance);
        double trend     = computeFailureTrend(_daily_fail_counts);
        double risk      = computeGlobalRisk(future_fail_windows, lookahead, trend);

        if (verbose)
            printf("    [RiskAware] cur=%d tol=%d lookahead=%d trend=%.2f risk=%.3f\n",
                   cur, tolerance, lookahead, trend, risk);

        if (risk >= risk_threshold_partial) {
            _risk_sum_at_repair += risk;
            _repair_trigger_count++;
        }

        if (risk >= risk_threshold_full)    return REPAIR_IMMEDIATE;
        if (risk >= risk_threshold_partial) return REPAIR_PARTIAL;
        _stats.skipped_repair_count++;
        return REPAIR_LAZY;
    }

    // ------------------------------------------------------------------
    // Strategy 4: StripeDifferentiated
    // ------------------------------------------------------------------
    pair<RepairDecision, vector<int>> decideStripeDifferentiated(
            const vector<Stripe*>& stripe_list,
            const vector<int>& next_fail_ids,
            double stripe_risk_threshold = 0.75)
    {
        int cur = (int)_current_failed.size();
        if (cur == 0) return {REPAIR_LAZY, {}};

        vector<StripeRisk> stripe_risks = computeStripeRisks(stripe_list);

        // build stripe_id -> Stripe* map for safe lookup
        unordered_map<int, Stripe*> sid_map_sd;
        for (Stripe* s : stripe_list)
            sid_map_sd[s->getStripeId()] = s;

        vector<StripeRisk> critical_stripes, high_risk_stripes;
        for (auto& sr : stripe_risks) {
            if (sr.critical) {
                critical_stripes.push_back(sr);
            } else if (sr.risk_score >= stripe_risk_threshold) {
                int future_hits = 0;
                auto sit = sid_map_sd.find(sr.stripe_id);
                if (sit == sid_map_sd.end()) continue;
                const vector<int>& pl = sit->second->getPlacement();
                for (int nid : next_fail_ids)
                    if (find(pl.begin(), pl.end(), nid) != pl.end())
                        future_hits++;
                if (sr.fail_count + future_hits > tolerance)
                    critical_stripes.push_back(sr);
                else
                    high_risk_stripes.push_back(sr);
            }
        }

        if (critical_stripes.empty() && high_risk_stripes.empty()) {
            _stats.skipped_repair_count++;
            return {REPAIR_LAZY, {}};
        }

        vector<NodeRepairPriority> priorities = computeNodePriorities(
            _current_failed, stripe_list, critical_stripes, high_risk_stripes);
        vector<int> nodes_to_repair = greedySelectNodes(
            priorities, critical_stripes, stripe_list);

        if (nodes_to_repair.empty()) {
            _stats.skipped_repair_count++;
            return {REPAIR_LAZY, {}};
        }
        if (nodes_to_repair.size() == _current_failed.size())
            return {REPAIR_IMMEDIATE, nodes_to_repair};
        return {REPAIR_PARTIAL, nodes_to_repair};
    }

    // ------------------------------------------------------------------
    // Strategy 5: Combined
    // ------------------------------------------------------------------
    pair<RepairDecision, vector<int>> decideCombined(
            const vector<Stripe*>& stripe_list,
            const vector<vector<int>>& future_fail_windows,
            double risk_threshold_partial = 0.5,
            double risk_threshold_full    = 0.8,
            double stripe_risk_threshold  = 0.75)
    {
        int cur = (int)_current_failed.size();
        if (cur == 0) return {REPAIR_LAZY, {}};

        int    lookahead   = adaptiveLookahead(cur, tolerance);
        double trend       = computeFailureTrend(_daily_fail_counts);
        double global_risk = computeGlobalRisk(future_fail_windows, lookahead, trend);

        if (verbose)
            printf("    [Combined] cur=%d tol=%d lookahead=%d trend=%.2f global_risk=%.3f\n",
                   cur, tolerance, lookahead, trend, global_risk);

        if (global_risk < risk_threshold_partial * 0.5) {
            _stats.skipped_repair_count++;
            return {REPAIR_LAZY, {}};
        }

        vector<StripeRisk> stripe_risks = computeStripeRisks(stripe_list);
        vector<int> next_fails = future_fail_windows.empty() ? vector<int>{} : future_fail_windows[0];

        // build stripe_id -> Stripe* map for safe lookup
        unordered_map<int, Stripe*> sid_map;
        for (Stripe* s : stripe_list)
            sid_map[s->getStripeId()] = s;

        vector<StripeRisk> must_repair_stripes;
        for (auto& sr : stripe_risks) {
            if (sr.critical) {
                must_repair_stripes.push_back(sr);
                continue;
            }
            int future_hits = 0;
            auto sit = sid_map.find(sr.stripe_id);
            if (sit == sid_map.end()) continue;
            for (int i = 0; i < (int)future_fail_windows.size() && i < lookahead; i++) {
                const vector<int>& pl = sit->second->getPlacement();
                for (int nid : future_fail_windows[i])
                    if (find(pl.begin(), pl.end(), nid) != pl.end())
                        future_hits++;
            }
            if (sr.fail_count + future_hits > tolerance)
                must_repair_stripes.push_back(sr);
        }

        if (must_repair_stripes.empty()) {
            if (global_risk >= risk_threshold_full) {
                _risk_sum_at_repair += global_risk;
                _repair_trigger_count++;
                return {REPAIR_IMMEDIATE, _current_failed};
            }
            _stats.skipped_repair_count++;
            return {REPAIR_LAZY, {}};
        }

        vector<StripeRisk> all_high_risk;
        for (auto& sr : stripe_risks)
            if (sr.risk_score >= stripe_risk_threshold)
                all_high_risk.push_back(sr);

        vector<NodeRepairPriority> priorities = computeNodePriorities(
            _current_failed, stripe_list, must_repair_stripes, all_high_risk);
        vector<int> nodes_to_repair = greedySelectNodes(
            priorities, must_repair_stripes, stripe_list);

        _risk_sum_at_repair += global_risk;
        _repair_trigger_count++;

        if (nodes_to_repair.empty() || nodes_to_repair.size() == _current_failed.size())
            return {REPAIR_IMMEDIATE, _current_failed};
        return {REPAIR_PARTIAL, nodes_to_repair};
    }

private:
    // ------------------------------------------------------------------
    // 计算每个Stripe的风险分
    // ------------------------------------------------------------------
    vector<StripeRisk> computeStripeRisks(const vector<Stripe*>& stripe_list) {
        vector<StripeRisk> risks;
        for (Stripe* s : stripe_list) {
            const vector<int>& placement = s->getPlacement();
            int fail_count = 0;
            vector<int> failed_in_stripe;
            for (int nid : _current_failed) {
                if (find(placement.begin(), placement.end(), nid) != placement.end()) {
                    fail_count++;
                    failed_in_stripe.push_back(nid);
                }
            }
            if (fail_count == 0) continue;

            StripeRisk sr;
            sr.stripe_id    = s->getStripeId();
            sr.fail_count   = fail_count;
            sr.tolerance    = tolerance;
            sr.risk_score   = (double)fail_count / tolerance;
            sr.critical     = (fail_count > tolerance);
            sr.failed_nodes = failed_in_stripe;
            risks.push_back(sr);
        }
        return risks;
    }

    // ------------------------------------------------------------------
    // 计算节点修复优先级
    // ------------------------------------------------------------------
    vector<NodeRepairPriority> computeNodePriorities(
            const vector<int>& failed_nodes,
            const vector<Stripe*>& stripe_list,
            const vector<StripeRisk>& critical_stripes,
            const vector<StripeRisk>& high_risk_stripes)
    {
        unordered_map<int, bool> critical_map, high_map;
        for (auto& sr : critical_stripes)  critical_map[sr.stripe_id] = true;
        for (auto& sr : high_risk_stripes) high_map[sr.stripe_id]     = true;

        vector<NodeRepairPriority> priorities;
        for (int nid : failed_nodes) {
            NodeRepairPriority np;
            np.node_id           = nid;
            np.critical_stripes  = 0;
            np.high_risk_stripes = 0;

            for (Stripe* s : stripe_list) {
                const vector<int>& pl = s->getPlacement();
                if (find(pl.begin(), pl.end(), nid) == pl.end()) continue;
                int sid = s->getStripeId();
                if (critical_map.count(sid)) np.critical_stripes++;
                if (high_map.count(sid))     np.high_risk_stripes++;
            }

            np.priority_score = np.critical_stripes * 10.0 + np.high_risk_stripes * 1.0;
            priorities.push_back(np);
        }

        sort(priorities.begin(), priorities.end(),
             [](const NodeRepairPriority& a, const NodeRepairPriority& b) {
                 return a.priority_score > b.priority_score;
             });
        return priorities;
    }

    // ------------------------------------------------------------------
    // 贪心选择最少节点覆盖所有critical stripe
    // ------------------------------------------------------------------
    vector<int> greedySelectNodes(
            const vector<NodeRepairPriority>& priorities,
            const vector<StripeRisk>& critical_stripes,
            const vector<Stripe*>& stripe_list)
    {
        unordered_map<int, int> uncovered;
        for (auto& sr : critical_stripes)
            uncovered[sr.stripe_id] = max(sr.fail_count - tolerance + 1, 1);

        unordered_map<int, Stripe*> stripe_map;
        for (Stripe* s : stripe_list)
            stripe_map[s->getStripeId()] = s;

        vector<int> selected;
        for (auto& np : priorities) {
            if (uncovered.empty()) break;
            if (np.priority_score == 0.0) break;

            int nid = np.node_id;
            bool useful = false;

            for (auto it = uncovered.begin(); it != uncovered.end(); ) {
                auto sit = stripe_map.find(it->first);
                if (sit == stripe_map.end()) { ++it; continue; }

                const vector<int>& pl = sit->second->getPlacement();
                if (find(pl.begin(), pl.end(), nid) != pl.end()) {
                    useful = true;
                    it->second--;
                    it = (it->second <= 0) ? uncovered.erase(it) : next(it);
                } else {
                    ++it;
                }
            }
            if (useful) selected.push_back(nid);
        }
        return selected;
    }

    // ------------------------------------------------------------------
    // 多步折扣全局风险分
    // ------------------------------------------------------------------
    double computeGlobalRisk(const vector<vector<int>>& future_windows,
                              int lookahead, double trend)
    {
        int    cur        = (int)_current_failed.size();
        double risk       = 0.0;
        double discount   = 1.0;
        int    cumulative = cur;

        for (int i = 0; i < (int)future_windows.size() && i < lookahead; i++) {
            cumulative += (int)future_windows[i].size();
            double step_risk = (double)cumulative / tolerance;
            risk += discount * min(step_risk, 1.0);
            discount *= 0.7;
        }

        double max_risk = 0.0, d = 1.0;
        for (int i = 0; i < lookahead; i++) { max_risk += d; d *= 0.7; }
        risk = (max_risk > 0) ? risk / max_risk : 0.0;

        double trend_factor = min(trend / max((double)tolerance * 0.5, 1.0), 0.3);
        return min(risk + trend_factor, 1.0);
    }
};
