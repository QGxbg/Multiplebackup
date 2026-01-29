#ifndef _ECDAG_HH_
#define _ECDAG_HH_

#include "../inc/include.hh"

#include "ECNode.hh"
//#include "ECUnit.hh"
//#include "ECCluster.hh"
//#include "ECTask.hh"
//#include "../util/BlockingQueue.hh"
//#include "../util/LoadVector.hh"

using namespace std;

#define ECDAG_DEBUG_ENABLE false
// FIXME:
//  32767-1   32767-2    ......  
// multiple failure 染色的例子
#define REQUESTOR 32767
#define SGSTART 0
#define USTART 0
#define CSTART 0

class ECDAG {
  private:


    //// for coloring
    //unordered_map<int, unsigned int> _idx2ip;

    //// for ECUnits
    //int _unitId = USTART;
    //unordered_map<int, ECUnit*> _ecUnitMap;
    //vector<int> _ecUnitList;

    //// for ECClusters
    //int _clusterId = CSTART;
    //unordered_map<int, ECCluster*> _ecClusterMap;
    //vector<int> _ecClusterList;
    
  public:
  
    ECDAG(); 
    ~ECDAG();
    vector<int> _ecConcacts; // concact(blkdix) = REQUESTOR - blkidx
    void Join(int pidx, vector<int> cidx, vector<int> coefs);
    void Concact(vector<int> cidx);
    vector<int> genItmIdxs();

    void Concact(vector<int> cidx, int n, int w);

    void removeChild(ECNode* pnode);
    
    unordered_map<int, ECNode*> _ecNodeMap;
    vector<int> _ecHeaders;
    vector<int> _ecLeaves;


    unordered_map<int, ECNode*> getECNodeMap();
    unordered_map<int, ECNode*>& getECNodeMapNew();
    vector<int> getECHeaders();
    vector<int> getECLeaves();
    vector<int> getAllNodeIds();
    //unordered_map<int, ECUnit*> getUnitMap();
    //vector<int> getUnitList();

    // for debug
    void dump();
    //void refreshECDAG(ECDAG * ecdag);


    vector<vector<int>> genLeveledTopologicalSorting();

    // han add
    vector<int> genTopoIdxs();
    void dumpTOPO();
    // end

};
#endif
