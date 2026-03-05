#ifndef _REPAIRSCHEDULER_HH_
#define _REPAIRSCHEDULER_HH_

#include <vector>
#include <set>
#include <string>
#include <iostream>
#include "TraceReader.hh"

using namespace std;

enum RepairDecision {
    REPAIR_IMMEDIATE,   // 立即修复
    REPAIR_LAZY,        // 延迟修复
    REPAIR_PARTIAL      // 部分修复（新增）
};

struct RepairStats {
    int total_repair_rounds;       // 总修复轮次
    double total_repair_load;      // 总修复负载(blocks)
    double total_repair_bdwt;      // 总修复带宽(blocks)
    int data_loss_events;          // 数据丢失事件数
    int lazy_skip_count;           // 延迟跳过次数
    int max_concurrent_failures;   // 最大同时故障数
    // 新增字段
    int partial_repair_count = 0;     // 部分修复次数
    int skipped_repair_count = 0;     // 跳过修复次数
    double avg_risk_at_repair = 0.0;  // 触发修复时的平均风险分
};

class RepairScheduler {
    int _ecn;
    int _eck;
    int _ecw;
    int _fault_tolerance;          // ecn - eck, 最大容错数
    set<int> _current_failed_nodes;
    RepairStats _stats;

public:
    RepairScheduler(int ecn, int eck, int ecw);

    // 添加故障节点, 返回是否造成数据丢失(超过容错上限)
    bool addFailures(vector<int> fail_node_ids);

    // ====== 三种策略 ======
    // Immediate: 只要有故障就立即修复
    RepairDecision decideImmediate();

    // Lazy: 等到故障数达到容错上限才修复
    RepairDecision decideLazy();

    // Trace-Driven: 根据下一时间段预测决定
    RepairDecision decideTraceDriven(TraceEvent* next_event);

    // 执行修复: 清空故障节点, 返回修复的节点列表
    vector<int> executeRepair();

    // 获取当前故障节点列表
    vector<int> getCurrentFailedNodes();

    // 获取当前故障数
    int getCurrentFailureCount();

    // 记录本次修复的负载
    void recordRepairLoad(double load, double bdwt);

    // 记录数据丢失
    void recordDataLoss();

    // 获取统计信息
    RepairStats getStats();

    // 重置调度器(切换策略时使用)
    void reset();

    // 获取容错能力
    int getFaultTolerance();

    // 打印当前状态
    void printStatus(string strategy_name);
};

#endif
