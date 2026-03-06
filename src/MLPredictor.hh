#ifndef _MLPREDICTOR_HH_
#define _MLPREDICTOR_HH_

// MLPredictor.hh
// 模拟基于ML的故障预测器，在真实trace基础上注入误报/漏报
//
// 参数来源（论文数据）：
//   false_positive_rate: 0.002~0.025  (0.2%~2.5%)
//   false_negative_rate: 0.05         (1 - 95%精度)
//
// 使用方式：
//   MLPredictor predictor(total_nodes, fpr, fnr, seed);
//   predictor.setNodePool(all_node_ids);
//
//   // 预测"下一时间窗口"的故障节点（用于修复决策）
//   vector<int> predicted = predictor.predictNextFailures(real_next_event);

#include "common/TraceReader.hh"
#include <vector>
#include <random>
#include <algorithm>
#include <unordered_set>
#include <iostream>

using namespace std;

// 预测质量统计，用于实验报告
struct PredictorStats
{
    int true_positives = 0;  // 正确预测到的故障
    int false_positives = 0; // 误报：预测故障但实际没故障
    int false_negatives = 0; // 漏报：实际故障但没预测到
    int true_negatives = 0;  // 正确预测为正常

    double precision() const
    {
        int denom = true_positives + false_positives;
        return denom > 0 ? 1.0 * true_positives / denom : 1.0;
    }
    double recall() const
    {
        int denom = true_positives + false_negatives;
        return denom > 0 ? 1.0 * true_positives / denom : 1.0;
    }
    double fpr() const
    {
        int denom = false_positives + true_negatives;
        return denom > 0 ? 1.0 * false_positives / denom : 0.0;
    }
};

class MLPredictor
{
public:
    int _total_nodes; // 集群总节点数
    double _fpr;      // 误报率 false positive rate
    double _fnr;      // 漏报率 false negative rate (= 1 - recall)
    int _lookahead;   // 预测未来几个时间步（默认1）

    vector<int> _all_node_ids; // 所有节点ID
    PredictorStats _stats;

    mt19937 _rng;

    // ------------------------------------------------------------------
    // 构造：total_nodes=集群节点数, fpr=误报率, fnr=漏报率, seed=随机种子
    // ------------------------------------------------------------------
    MLPredictor(int total_nodes, double fpr, double fnr, int seed = 42)
        : _total_nodes(total_nodes), _fpr(fpr), _fnr(fnr),
          _lookahead(1), _rng(seed)
    {
        // 默认节点ID为 0..total_nodes-1
        for (int i = 0; i < total_nodes; i++)
            _all_node_ids.push_back(i);
    }

    // 允许外部设置真实节点ID列表（如果ID不连续）
    void setNodePool(const vector<int> &node_ids)
    {
        _all_node_ids = node_ids;
        _total_nodes = node_ids.size();
    }

    void setLookahead(int l) { _lookahead = l; }

    // ------------------------------------------------------------------
    // 核心接口：给定真实的下一事件，返回ML模型预测的故障节点列表
    //
    // 模拟过程：
    //   对每个真实故障节点：以 (1-fnr) 概率保留（正确预测），fnr概率漏掉
    //   对每个正常节点：  以 fpr 概率误报为故障
    // ------------------------------------------------------------------
    vector<int> predictNextFailures(TraceEvent *real_next_event,
                                    const vector<int> &current_failed = {})
    {
        // 真实下一步会故障的节点集合
        unordered_set<int> real_fail_set;
        if (real_next_event)
        {
            for (int nid : real_next_event->fail_node_ids)
                real_fail_set.insert(nid);
        }

        // 当前已故障节点（排除在预测范围外）
        unordered_set<int> already_failed(current_failed.begin(), current_failed.end());

        vector<int> predicted;
        uniform_real_distribution<double> dist(0.0, 1.0);

        for (int nid : _all_node_ids)
        {
            if (already_failed.count(nid))
                continue; // 已故障节点不重复预测

            if (real_fail_set.count(nid))
            {
                // 真实会故障：以 (1-fnr) 概率预测为故障（正确），fnr概率漏报
                if (dist(_rng) >= _fnr)
                {
                    predicted.push_back(nid);
                    _stats.true_positives++;
                }
                else
                {
                    _stats.false_negatives++;
                }
            }
            else
            {
                // 真实不故障：以 fpr 概率误报为故障
                if (dist(_rng) < _fpr)
                {
                    predicted.push_back(nid);
                    _stats.false_positives++;
                }
                else
                {
                    _stats.true_negatives++;
                }
            }
        }

        return predicted;
    }

    // ------------------------------------------------------------------
    // 多步预测：返回未来 lookahead 步的预测故障列表
    // future_real_events: 真实的未来事件序列（ground truth，仅用于注入噪声）
    // ------------------------------------------------------------------
    vector<vector<int>> predictMultiStep(
        const vector<TraceEvent *> &future_real_events,
        const vector<int> &current_failed = {})
    {
        vector<vector<int>> result;
        unordered_set<int> cumulative_failed(current_failed.begin(), current_failed.end());

        for (int i = 0; i < (int)future_real_events.size() && i < _lookahead; i++)
        {
            vector<int> step_pred = predictNextFailures(
                future_real_events[i],
                vector<int>(cumulative_failed.begin(), cumulative_failed.end()));
            result.push_back(step_pred);

            // 把真实故障累积进去（模拟时间推进）
            if (future_real_events[i])
            {
                for (int nid : future_real_events[i]->fail_node_ids)
                    cumulative_failed.insert(nid);
            }
        }
        return result;
    }

    void resetStats() { _stats = PredictorStats{}; }

    PredictorStats getStats() const { return _stats; }

    void printStats() const
    {
        cout << "  MLPredictor Stats:" << endl;
        printf("    Config:    FPR=%.3f  FNR=%.3f\n", _fpr, _fnr);
        printf("    TP=%-6d FP=%-6d FN=%-6d TN=%-6d\n",
               _stats.true_positives, _stats.false_positives,
               _stats.false_negatives, _stats.true_negatives);
        printf("    Precision= %.3f  Recall= %.3f  Actual FPR= %.4f\n",
               _stats.precision(), _stats.recall(), _stats.fpr());
    }
};

#endif
