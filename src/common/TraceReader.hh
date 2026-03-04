#ifndef _TRACEREADER_HH_
#define _TRACEREADER_HH_

#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>

using namespace std;

struct TraceEvent {
    int timestamp;              // 时间戳(天)
    vector<int> fail_node_ids;  // 故障节点ID列表
};

class TraceReader {
    vector<TraceEvent> _events;
    int _current_idx;

public:
    TraceReader();

    // 从文件加载trace, 格式: timestamp num_failures node_id1 node_id2 ...
    bool loadTrace(string filepath);

    // 获取当前事件
    TraceEvent* getCurrentEvent();

    // 预览下一事件(不推进指针)
    TraceEvent* peekNextEvent();

    // 推进到下一事件
    void advance();

    // 重置到开头
    void reset();

    // 是否还有当前事件
    bool hasCurrentEvent();

    // 是否还有下一事件
    bool hasNextEvent();

    // 获取事件总数
    int getEventCount();
};

#endif
