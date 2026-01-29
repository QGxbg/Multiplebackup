#include "RDP.hh"

RDP::RDP(int n, int k, int w, vector<string> param) {
//RDP::RDP(int n, int k, int w, int opt, vector<string> param) {
    _n = n;
    _k = k;
    _w = w;
//    _opt = opt;

    _m = _n - k;
    _p = _k + 1;

    for(int rowid=0; rowid<_w; rowid++) {
        for (int colid=0; colid<_n; colid++) {
            int idx = colid * _w + rowid;
            int d = (rowid+colid) % _p;
            _idx2diag.insert(make_pair(idx, d));
            //cout << "symbol " << idx << ", (" << rowid << ", " << colid << "), diag " << d << endl;

            if (d >= _w)
                continue;

            if (idx < _p*_w) {
                // the current idx is a data idx
                if (_diag2data.find(d) != _diag2data.end()) {
                    _diag2data[d].push_back(idx);
                } else {
                    vector<int> curv = {idx};
                    _diag2data.insert(make_pair(d, curv));
                }
            } else {
                // the current idx is a code idx
                _diag2code.insert(make_pair(d, idx));
            }
        }
    }
}

ECDAG* RDP::Encode() {
    ECDAG* ecdag = new ECDAG();

    // 0. calculate the row parity sub-blocks
    for (int i=0; i<_w; i++) {
        int code = _k * _w + i;
        vector<int> data;
        vector<int> coef;
        for (int j=0; j<_k; j++) {
            int idx = j * _w + i;
            data.push_back(idx);
            coef.push_back(1);
        }
        ecdag->Join(code, data, coef);
    }

    // 1. calculate the diagonal parity sub-blocks
    for (auto item : _diag2code) {
        int d = item.first;
        int code = item.second;
        vector<int> data = _diag2data[d];
        vector<int> coef;
        for (int i=0; i<data.size(); i++)
            coef.push_back(1);
        ecdag->Join(code, data, coef);
        
        //cout << "code " << code << ", data ( ";
        //for (auto i: data) {
        //    cout << i << " ";
        //}
        //cout << ")" << endl;
    }

    return ecdag;
}

ECDAG* RDP::Decode(vector<int> from, vector<int> to) {
    ECDAG* ecdag = new ECDAG();

    // make sure that idx in to are in the same node
    int fnodeidx = to[0] / _w;
    bool canrepair = true;
    for (int i=1; i<to.size(); i++) {
        if (to[i]/_w != fnodeidx) {
            canrepair = false;
            break;
        }
    }

    if (!canrepair) {
        cout << "ERROR::RDP only repairs single failures!" << endl;
        return NULL;
    }

    if (fnodeidx != _n-1) {
        // repair by row parity sub-blocks
        vector<int> repairlist;
        for (int i=0; i<_w; i++) {
            int torepair = fnodeidx * _w + i;
            repairlist.push_back(torepair);
            vector<int> data;
            vector<int> coef;
            for(int j=0; j<_p; j++) {
                if (j == fnodeidx)
                    continue;
                int curidx = j*_w + i;
                data.push_back(curidx);
                coef.push_back(1);
            }
            ecdag->Join(torepair, data, coef);
        }
        //ecdag->BindX(repairlist);
    } else {
        // repair by diag parity sub-blocks
        int colid = _n-1;
        vector<int> repairlist;
        for (int rowid=0; rowid<_w; rowid++) {
            int repairidx = colid*_w+rowid;
            repairlist.push_back(repairidx);
            int d = (colid + rowid) % _p;
            vector<int> data = _diag2data[d];
            vector<int> coef;
            for (int i=0; i<data.size(); i++)
                coef.push_back(1);
            ecdag->Join(repairidx, data, coef);
        }
        //ecdag->BindX(repairlist);
    }

    return ecdag;
}

void RDP::Place(vector<vector<int>>& group) {

}

