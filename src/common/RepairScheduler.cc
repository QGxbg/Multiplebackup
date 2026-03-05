#include "RepairScheduler.hh"

RepairScheduler::RepairScheduler(int ecn, int eck, int ecw) {
    _ecn = ecn;
    _eck = eck;
    _ecw = ecw;
    _fault_tolerance = ecn - eck;
    _stats = RepairStats{};
}

bool RepairScheduler::addFailures(vector<int> fail_node_ids) {
    for (int id : fail_node_ids) {
        _current_failed_nodes.insert(id);
    }
    int cur = _current_failed_nodes.size();
    if (cur > _stats.max_concurrent_failures) {
        _stats.max_concurrent_failures = cur;
    }
    // 超过容错上限 -> 数据丢失
    return cur > _fault_tolerance;
}

RepairDecision RepairScheduler::decideImmediate() {
    // 只要有故障就修复
    if (_current_failed_nodes.size() > 0) {
        return REPAIR_IMMEDIATE;
    }
    return REPAIR_LAZY;
}

RepairDecision RepairScheduler::decideLazy() {
    // 等到故障数达到容错上限才修复
    if ((int)_current_failed_nodes.size() >= _fault_tolerance) {
        return REPAIR_IMMEDIATE;
    }
    return REPAIR_LAZY;
}

RepairDecision RepairScheduler::decideTraceDriven(TraceEvent* next_event) {
    int current_failures = _current_failed_nodes.size();
    int remaining_tolerance = _fault_tolerance - current_failures;

    if (remaining_tolerance <= 0) {
        // 已经没有容错空间, 必须立即修复
        return REPAIR_IMMEDIATE;
    }

    if (next_event == nullptr) {
        // 没有下一事件了, 当前有故障就修复
        if (current_failures > 0) {
            return REPAIR_IMMEDIATE;
        }
        return REPAIR_LAZY;
    }

    // 预测: 下一时间段的故障数 >= 剩余容错空间 -> 必须修复
    int next_failures = next_event->fail_node_ids.size();
    if (next_failures > remaining_tolerance) {
        return REPAIR_IMMEDIATE;
    }

    return REPAIR_LAZY;
}

vector<int> RepairScheduler::executeRepair() {
    vector<int> repaired(_current_failed_nodes.begin(), _current_failed_nodes.end());
    _current_failed_nodes.clear();
    _stats.total_repair_rounds++;
    return repaired;
}

vector<int> RepairScheduler::getCurrentFailedNodes() {
    return vector<int>(_current_failed_nodes.begin(), _current_failed_nodes.end());
}

int RepairScheduler::getCurrentFailureCount() {
    return _current_failed_nodes.size();
}

void RepairScheduler::recordRepairLoad(double load, double bdwt) {
    _stats.total_repair_load += load;
    _stats.total_repair_bdwt += bdwt;
}

void RepairScheduler::recordDataLoss() {
    _stats.data_loss_events++;
}

RepairStats RepairScheduler::getStats() {
    return _stats;
}

void RepairScheduler::reset() {
    _current_failed_nodes.clear();
    _stats = RepairStats{};
}

int RepairScheduler::getFaultTolerance() {
    return _fault_tolerance;
}

void RepairScheduler::printStatus(string strategy_name) {
    cout << "[" << strategy_name << "] "
         << "current_failures=" << _current_failed_nodes.size()
         << ", fault_tolerance=" << _fault_tolerance
         << ", remaining=" << _fault_tolerance - (int)_current_failed_nodes.size()
         << endl;
}
