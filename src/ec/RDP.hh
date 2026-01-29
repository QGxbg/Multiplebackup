#ifndef _RDP_HH_
#define _RDP_HH_

#include "../inc/include.hh"
#include "Computation.hh"

#include "ECBase.hh"

using namespace std;

#define RDPBASE_DEBUG_ENABLE true 

class RDP : public ECBase {
    private:
        int _m;
        int _p;

        unordered_map<int, int> _idx2diag;
        unordered_map<int, vector<int>> _diag2data;
        unordered_map<int, int> _diag2code;

    public:
        //RDP(int n, int k, int w, int opt, vector<string> param);
        RDP(int n, int k, int w, vector<string> param);

        ECDAG* Encode();
        ECDAG* Decode(vector<int> from, vector<int> to);
        void Place(vector<vector<int>>& group);
};

#endif
