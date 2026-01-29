#include "PRSolution.hh"

PRSolution::PRSolution(long long id, int v, int m) {
    _id = id;
    _v = v;
    _m = m;

    _searched = false;

    srand(time(NULL));
    //srand(1);
    for (int i=0; i<v; i++) {
        int r = rand() % _m;
        _solution.push_back(r);    
    }

    for (int i=0; i<_solution.size(); i++)
        cout << _solution[i] << " ";
    cout << endl;

    _bdwt = 0;
    _load = 0;

    _h_bdwt = 0;
    _h_load = 0;
}

PRSolution::PRSolution(long long id, int v, int m, vector<int> sol) {
    _id = id;
    _v = v;
    _m = m;

    for (int i=0; i<v; i++)
        _solution.push_back(sol[i]);

    _searched = false;
    _bdwt = 0;
    _load = 0;

    _h_bdwt = 0;
    _h_load = 0;
}

PRSolution::PRSolution(long long id, int v, vector<vector<int>> rackinfo, int digits) {
    _id = id;
    _v = v;
    _m = digits;

    _searched = false;

    srand(time(NULL));
    //srand(1);
    for (int i=0; i<v; i++) {
        vector<int> candidates = rackinfo[i];
        int r = rand() % candidates.size();
        _solution.push_back(candidates[r]);    
    }

    _bdwt = 0;
    _load = 0;

    _h_bdwt = 0;
    _h_load = 0;
}

//void PRSolution::evaluate(ECDAG* ecdag, unordered_map<int, int> sid2color, vector<int> itm_idx) {
//
//    struct timeval time1, time2;
//    gettimeofday(&time1, NULL);
//
//    unordered_map<int, int> coloring_res;
//    for (auto item: sid2color) {
//        coloring_res.insert(make_pair(item.first, item.second));
//    }
//    
//    for (int ii=0; ii<_solution.size(); ii++) {
//        int idx = itm_idx[ii];
//        int color = _solution[ii];
//        coloring_res[idx] = color;
//    }
//
//    ecdag->clearECCluster();
//    ecdag->genECCluster(coloring_res, 32768);
//
//    unordered_map<int, int> inmap;
//    unordered_map<int, int> outmap;
//    ecdag->genStat(coloring_res, inmap, outmap);
//
//    int bw=0, max=0;
//    for (auto item: inmap) {
//        bw+= item.second;
//        if (item.second > max)
//            max = item.second;
//    }
//    for (auto item: outmap) {
//        if (item.second > max)
//            max = item.second;
//    }
//
//    _load = max;
//    _bdwt = bw;
//    gettimeofday(&time2, NULL);
//    double latency = DistUtil::duration(time1, time2);
//    cout << "stat: " << latency << endl;
//}

void PRSolution::evaluate(ECDAG* ecdag, unordered_map<int, int> sid2color, vector<int> itm_idx) {

    struct timeval time1, time2;
    gettimeofday(&time1, NULL);

    // get full coloring
    unordered_map<int, int> coloring;

    for (auto item: sid2color) {
        int idx = item.first;
        int color = item.second;
        coloring.insert(make_pair(idx, color));
    }

    for (int i=0; i<itm_idx.size(); i++) {
        int idx = itm_idx[i];
        int color = _solution[i];

        if (coloring.find(idx) == coloring.end())
            coloring.insert(make_pair(idx, color));
        else
            coloring[idx] = color;
    }

    // debug coloring
    //cout << "Debug the coloring: " << endl;
    for (auto item: coloring) {
        int idx = item.first;
        int color = item.second;
        //cout << "idx: " << idx << ", color: " << color << endl;
    }
    
    //ecdag->clearECCluster();
    //ecdag->genECCluster(coloring, 32768);

    // generate in and out table
    unordered_map<int, int> in;
    unordered_map<int, int> out;

    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();

    for (auto item: coloring) {
        int idx = item.first;
        int mycolor = item.second;

        if (mycolor == -1)
            continue;

        ECNode* node = ecNodeMap[idx];
        vector<ECNode*> parents = node->getParentNodes();

        vector<int> parent_colors;
        for (int i=0; i<parents.size(); i++) {
            ECNode* parentnode = parents[i];
            int parentidx = parentnode->getNodeId();
            int parentcolor = coloring[parentidx];

            if (find(parent_colors.begin(), parent_colors.end(), parentcolor) == parent_colors.end())
                parent_colors.push_back(parentcolor);
        }

        for (int i=0; i<parent_colors.size(); i++) {
            if (mycolor != parent_colors[i]) {
                if (out.find(mycolor) == out.end())
                    out.insert(make_pair(mycolor, 1));
                else
                    out[mycolor]++;

                if (in.find(parent_colors[i]) == in.end())
                    in.insert(make_pair(parent_colors[i], 1));
                else
                    in[parent_colors[i]]++;
            }
        }
    }

    // // debug in table
    // cout << "in table: " << endl;
    // for (auto item: in) {
    //     int idx = item.first;
    //     int num = item.second;
    //     cout << idx << ": " << num << endl;
    // }

    // // debug out table
    // cout << "out table: " << endl;
    // for (auto item: out) {
    //     int idx = item.first;
    //     int num = item.second;
    //     cout << idx << ": " << num << endl;
    // }

    // calculate bdwt and load
    for (auto item: in) {
        _bdwt += item.second;
        if (item.second > _load)
            _load = item.second;
    }

    for (auto item: out) {
        if (item.second > _load)
            _load = item.second;
    }

    if (_load == 0)
        cout << "error!" << endl;

    gettimeofday(&time2, NULL);
    double latency = DistUtil::duration(time1, time2);
    //cout << "stat time: " << latency << endl;
}

void PRSolution::evaluateWithHierarchy(ECDAG* ecdag, unordered_map<int, int> sid2color, vector<int> itm_idx, unordered_map<int, int> bidx2rackid) {

    // get full rack coloring
    unordered_map<int, int> rcoloring;
    unordered_map<int, int> ncoloring;

    for (auto item: sid2color) {
        int idx = item.first;
        int ncolor = item.second;
        int rcolor = -1;
        if (bidx2rackid.find(ncolor) != bidx2rackid.end())
            rcolor = bidx2rackid[ncolor];
        rcoloring.insert(make_pair(idx, rcolor));
        ncoloring.insert(make_pair(idx, ncolor));

        //cout << "idx: " << idx << ", ncolor: " << ncolor << ", rcolor: " << rcolor << endl;
    }

    for (int i=0; i<itm_idx.size(); i++) {
        int idx = itm_idx[i];
        int ncolor = _solution[i];
        int rcolor = -1;
        if (bidx2rackid.find(ncolor) != bidx2rackid.end())
            rcolor = bidx2rackid[ncolor];

        if (rcoloring.find(idx) == rcoloring.end())
            rcoloring.insert(make_pair(idx, rcolor));
        else
            rcoloring[idx] = rcolor;

        if (ncoloring.find(idx) == ncoloring.end())
            ncoloring.insert(make_pair(idx, ncolor));
        else
            ncoloring[idx] = ncolor;
        //cout << "i: " << i << ", rcolor: " << rcolor << ", ncolor: " << ncolor << endl;
    }

    //cout << "debug rcolor:" << endl;
    //for (auto item: rcoloring) {
    //    int idx = item.first;
    //    int rcolor = item.second;
    //    int ncolor = ncoloring[idx];
    //    cout << "  idx: " << idx << ", ncolor: " << ncolor << ", rcolor: " << rcolor << endl;
    //}

    // generate in and out table at the rack level
    unordered_map<int, int> in;
    unordered_map<int, int> out;

    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMapNew();

    for (auto item: rcoloring) {
        int idx = item.first;
        int mycolor = item.second;

        if (mycolor == -1)
            continue;

        ECNode* node = ecNodeMap[idx];
        vector<ECNode*> parents = node->getParentNodes();

        vector<int> parent_colors;
        for (int i=0; i<parents.size(); i++) {
            ECNode* parentnode = parents[i];
            int parentidx = parentnode->getNodeId();
            int parentcolor = rcoloring[parentidx];

            if (find(parent_colors.begin(), parent_colors.end(), parentcolor) == parent_colors.end())
                parent_colors.push_back(parentcolor);
        }

        for (int i=0; i<parent_colors.size(); i++) {
            if (mycolor != parent_colors[i]) {
                if (out.find(mycolor) == out.end())
                    out.insert(make_pair(mycolor, 1));
                else
                    out[mycolor]++;

                if (in.find(parent_colors[i]) == in.end())
                    in.insert(make_pair(parent_colors[i], 1));
                else
                    in[parent_colors[i]]++;
            }
        }
    }

    // calculate bdwt and load
    for (auto item: in) {
        _h_bdwt += item.second;
        if (item.second > _h_load)
            _h_load = item.second;
    }

    for (auto item: out) {
        if (item.second > _h_load)
            _h_load = item.second;
    }

    if (_h_load == 0)
        cout << "error!" << endl;

}

int PRSolution::getBdwt() {
    return _bdwt;
}

int PRSolution::getLoad() {
    return _load;
}

int PRSolution::getHierarchyBdwt() {
    return _h_bdwt;
}

int PRSolution::getHierarchyLoad() {
    return _h_load;
}

long long PRSolution::getId() {
    return _id;
}

vector<int> PRSolution::getSolution() {
    return _solution;
}

bool PRSolution::getSearched() {
    return _searched;
}

void PRSolution::setSearched() {
    _searched = true;
}

void PRSolution::setLoad(int load) {
    _load = load;
}

void PRSolution::setBdwt(int bdwt) {
    _bdwt = bdwt;
}

string PRSolution::getString() {
    // we transfer _solution to a string
    string toret;
    int digits = DistUtil::ndigits(_m);

    for (int i=0; i<_v; i++) {
        toret += DistUtil::num2str(_solution[i], digits);
    }

    //cout << "s: " << toret << endl;

    return toret;
}


string PRSolution::dump() {
    string toret = "solution: ";
    for (int i=0; i<_solution.size(); i++)
        toret += to_string(_solution[i]) + " ";
    toret += "\n";
    return toret;
}