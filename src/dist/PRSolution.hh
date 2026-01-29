#ifndef _PRSOLUTION_HH_
#define _PRSOLUTION_HH_

#include "../inc/include.hh"
#include "../util/DistUtil.hh"
#include "../ec/ECDAG.hh"

using namespace std;

class PRSolution {
    private:
        long long _id;

        // the number of intermediate vertices
        int _v; 
        // the number of colors
        int _m;
        // coloring solution
        vector<int> _solution;

        // per node
        int _bdwt;
        int _load;
        bool _searched;

        // under hierarchy architecture
        int _h_bdwt;
        int _h_load;

    public:
        PRSolution(long long id, int v, int m); // generate a random solution
        PRSolution(long long id, int v, int m, vector<int> sol);
        PRSolution(long long id, int v, vector<vector<int>> rackinfo, int digits);

        void evaluate(ECDAG* ecdag, unordered_map<int, int> sid2color, vector<int> itm_idx);
        void evaluateWithHierarchy(ECDAG* ecdag, unordered_map<int, int> sid2color, vector<int> idm_idx, unordered_map<int, int> sidx2rackid);
        int getBdwt();
        int getLoad();
        int getHierarchyBdwt();
        int getHierarchyLoad();
        long long getId();
        vector<int> getSolution();
        bool getSearched();
        void setSearched();
        string getString();
        void setLoad(int load);
        void setBdwt(int bdwt);

        string dump();

};

#endif