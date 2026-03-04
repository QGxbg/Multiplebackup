#include "TraceReader.hh"

TraceReader::TraceReader() : _current_idx(0) {}

bool TraceReader::loadTrace(string filepath) {
    ifstream infile(filepath);
    if (!infile.is_open()) {
        cerr << "TraceReader: Failed to open trace file: " << filepath << endl;
        return false;
    }

    string line;
    while (getline(infile, line)) {
        // 跳过空行和注释行
        if (line.empty() || line[0] == '#') continue;

        istringstream iss(line);
        TraceEvent event;
        int num_failures;

        iss >> event.timestamp >> num_failures;
        for (int i = 0; i < num_failures; i++) {
            int node_id;
            iss >> node_id;
            event.fail_node_ids.push_back(node_id);
        }

        _events.push_back(event);
    }

    infile.close();
    _current_idx = 0;
    cout << "TraceReader: Loaded " << _events.size() << " events from " << filepath << endl;
    return true;
}

TraceEvent* TraceReader::getCurrentEvent() {
    if (_current_idx < _events.size()) {
        return &_events[_current_idx];
    }
    return nullptr;
}

TraceEvent* TraceReader::peekNextEvent() {
    if (_current_idx + 1 < _events.size()) {
        return &_events[_current_idx + 1];
    }
    return nullptr;
}

void TraceReader::advance() {
    if (_current_idx < _events.size()) {
        _current_idx++;
    }
}

void TraceReader::reset() {
    _current_idx = 0;
}

bool TraceReader::hasCurrentEvent() {
    return _current_idx < _events.size();
}

bool TraceReader::hasNextEvent() {
    return (_current_idx + 1) < _events.size();
}

int TraceReader::getEventCount() {
    return _events.size();
}
