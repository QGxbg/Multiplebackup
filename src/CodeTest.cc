#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/ECBase.hh"
#include "ec/Clay.hh"
#include "ec/RDP.hh"
#include "ec/HHXORPlus.hh"
#include "ec/BUTTERFLY.hh"
#include "ec/RSCONV.hh"
#include "ec/RSPIPE.hh"
#include "sol/SolutionBase.hh"
#include "sol/OfflineSolution.hh"
#include "sol/ParallelSolution.hh"

using namespace std;

void usage() {
    cout << "usage: ./CodeTest" << endl;
    cout << "   1. code [Clay|RDP|HHXORPlus|BUTTERFLY|RSCONV|RSPIPE]" << endl;
    cout << "   2. n [14]" << endl;
    cout << "   3. k [10]" << endl;
    cout << "   4. w [256]" << endl;
    cout << "   5. fail node id [0]" << endl;
    cout << "   6. method [offline|parallel]" << endl;
}

vector<ComputeItem*> getEncodeList(ECDAG* ecdag) {
    vector<ComputeItem*> toret;

    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMap();

    vector<vector<int>> topolevel = ecdag->genLeveledTopologicalSorting();
    int num_level = topolevel.size();

    for (int level=1; level<num_level; level++) {
        vector<int> dagidxlist = topolevel[level];
        for (auto dagidx: dagidxlist) {
            ECNode* curnode = ecNodeMap[dagidx];
            vector<int> srclist = curnode->getChildIndices();
            vector<int> coefs = curnode->getCoefs();
            pair<bool, bool> usage = make_pair(true, true);
            ComputeItem* ci = new ComputeItem(dagidx, srclist, coefs, usage);
            toret.push_back(ci);
        }
    }

    return toret;
}

vector<ComputeItem*> getDecodeList(ECDAG* ecdag, unordered_map<int, int> coloring) {
    vector<ComputeItem*> toret;

    unordered_map<int, ECNode*> ecNodeMap = ecdag->getECNodeMap();

    vector<vector<int>> topolevel = ecdag->genLeveledTopologicalSorting();
    int num_level = topolevel.size();

    for (int level=1; level<num_level; level++) {
        vector<int> dagidxlist = topolevel[level];
        for (auto dagidx: dagidxlist) {
            assert(coloring.find(dagidx) != coloring.end());
            int mycolor = coloring[dagidx];

            if (mycolor == -1)
                continue;

            ECNode* curnode = ecNodeMap[dagidx];
            vector<int> srclist = curnode->getChildIndices();
            vector<int> coefs = curnode->getCoefs();
            pair<bool, bool> usage = make_pair(true, true);
            ComputeItem* ci = new ComputeItem(dagidx, srclist, coefs, usage);
            toret.push_back(ci);
        }
    }

    return toret;
}

int main(int argc, char** argv) {
    
    if (argc != 7) {
        usage();
        return 0;
    }

    string code = argv[1];
    int ecn = atoi(argv[2]);
    int eck = atoi(argv[3]);
    int ecw = atoi(argv[4]);
    int fnid = atoi(argv[5]);
    string method = argv[6];

    int pktbytes = 1024;
    int blkbytes = pktbytes * ecw;

    string configpath = "conf/sysSetting.xml";
    Config* conf = new Config(configpath);

    ECBase* ec;
    ECBase* dec;
    vector<string> param;

    if (code == "Clay") {
        ec = new Clay(ecn, eck, ecw, {to_string(ecn-1)});
        dec = new Clay(ecn, eck, ecw, {to_string(ecn-1)});
    } else if (code == "RDP") {
        ec = new RDP(ecn, eck, ecw, param);
        dec = new RDP(ecn, eck, ecw, param);
    } else if (code == "HHXORPlus") {
        ec = new HHXORPlus(ecn, eck, ecw, param);
        dec = new HHXORPlus(ecn, eck, ecw, param);
    } else if (code == "BUTTERFLY") {
        ec = new BUTTERFLY(ecn, eck, ecw, param);
        dec = new BUTTERFLY(ecn, eck, ecw, param);
    } else if (code == "RSCONV"){
        ec = new RSCONV(ecn,eck,ecw,param);
        dec = new RSCONV(ecn,eck,ecw,param);
    } else if (code == "RSPIPE"){
        ec = new RSPIPE(ecn,eck,ecw,param);
        dec = new RSPIPE(ecn,eck,ecw,param);
    }
    else{
        cout << "Non-supported code!" << endl;
        return 0;
    }

    // 0. prepare buffers
    char** buffers = (char**)calloc(ecn, sizeof(char*));
    for (int i=0; i<ecn; i++) {
        buffers[i] = (char*)calloc(blkbytes, sizeof(char));
        memset(buffers[i], 0, blkbytes);
        if (i < eck) {
            for (int j=0; j<ecw; j++) {
                int cid = i*ecw+j;
                memset(buffers[i] + pktbytes * j, cid, pktbytes);
            }
        } else {
            memset(buffers[i], 0, blkbytes);
        }
    }
    char* repairbuffer = (char*)calloc(blkbytes, sizeof(char));
    memset(repairbuffer, 0, blkbytes);

    // 1. encode
    ECDAG* encdag = nullptr;
    // 1.1 create encode tasks
    encdag = ec->Encode();
    vector<ComputeItem*> encodelist = getEncodeList(encdag);
    vector<int> tofree;
    // 1.2 prepare encoding bufMap
    vector<int> ecleaves = encdag->getECLeaves();
    unordered_map<int, char*> encBufMap;
    for (int i=0; i<ecn; i++) {
        for (int j=0; j<ecw; j++) {
            int dagidx = i*ecw+j;
            char* buf = buffers[i] + j*pktbytes;
            encBufMap.insert(make_pair(dagidx, buf));
        }
    }
    for (auto dagidx: ecleaves) {
        if (encBufMap.find(dagidx) == encBufMap.end()) {
            char* buf = (char*)calloc(pktbytes, sizeof(char));
            memset(buf, 0, pktbytes);
            encBufMap.insert(make_pair(dagidx, buf));
            tofree.push_back(dagidx);
        }
    }
    // 1.3 perform encoding
    for (int i=0; i<encodelist.size(); i++) {
        ComputeItem* ci = encodelist[i];
        vector<int> srclist = ci->_srclist;
        int dst = ci->_dstidx;
        vector<int> coefs = ci->_coefs;
        pair<bool, bool> usage = ci->_usage; 

        //cout << "dst: " << dst << ", srclist: " ;
        //for (auto srcdagidx: srclist)
        //    cout << srcdagidx << " ";
        //cout << endl;

        char** data = (char**)calloc(srclist.size(), sizeof(char*));
        char** code = (char**)calloc(1, sizeof(char*));
        for (int bufIdx = 0; bufIdx < srclist.size(); bufIdx++) {
            int child = srclist[bufIdx];
            assert (encBufMap.find(child) != encBufMap.end());
            data[bufIdx] = encBufMap[child];
        }
        if (encBufMap.find(dst) != encBufMap.end())
            code[0] = encBufMap[dst];
        else {
            code[0] = (char*)calloc(pktbytes, sizeof(char));
            memset(code[0], 0, pktbytes);
            encBufMap.insert(make_pair(dst, code[0]));
            tofree.push_back(dst);
        }

        int* matrix = (int*)calloc(srclist.size(), sizeof(int));
        for (int i=0; i<coefs.size(); i++) {
            matrix[i] = coefs[i];
        }

        Computation::Multi(code, data, matrix, 1, srclist.size(), pktbytes, "Isal");
        //cout << "    value: " << (int)code[0][0] << endl;

        //free(matrix);
        //free(code);
        //free(data);
    }
    //// 1.4 free buffers generated in encoding except for the ecn blocks
    //for (auto dagidx: tofree) {
    //    free(encBufMap[dagidx]);
    //}

//    // 1.5 debug encode
   for (int i=0; i<ecn; i++) {
       cout << "blk " << i << ": ";
       for (int j=0; j<ecw; j++) {
           char c = *(buffers[i]+pktbytes*j);
           cout << (int)c << " ";
       }
       cout << endl;
   }

    // 2. decode
    // 2.1 prepare stripe
    string stripename = "stripe0";
    vector<string> blklist;
    for (int i=0; i<ecn; i++) {
        string blkname = "blk"+to_string(i);
        blklist.push_back(blkname);              
    }
    vector<unsigned int> loclist;
    vector<int> nodelist;
    for (int i=0; i<ecn; i++) {
        unsigned int ip = conf->_agentsIPs[i];
        nodelist.push_back(i);
    }
    Stripe* stripe = new Stripe(0, stripename, blklist, loclist, nodelist);
    ECDAG* decdag = stripe->genRepairECDAG(dec, {fnid});

    decdag ->dump();
    decdag ->dumpTOPO();

    unordered_map<int, int> coloring;

    // 2.2 prepare coloring
    SolutionBase* sol;
    if (method == "offline") {
        sol = new OfflineSolution(1,1,1); //junlong add
        sol->init({stripe}, dec, code, conf);
        string offline_solution_path = conf->_tpDir+"/"+code+"_"+to_string(ecn)+"_"+to_string(eck)+"_"+to_string(ecw)+".xml";
        TradeoffPoints* tp = new TradeoffPoints(offline_solution_path);
        OfflineSolution* os = (OfflineSolution*)sol;
        os->setTradeoffPoints(tp);
        //os->genOfflineColoringForSingleFailure(stripe, coloring, fnid, conf->_agentsIPs.size(), "scatter");
        os->genOfflineColoringForSingleFailure(stripe, coloring, fnid, 14, "scatter");
        cout<<"2.2 prepare coloring end;"<<endl;
    } else if (method == "parallel") {
        sol = new ParallelSolution();
        sol->init({stripe}, dec, code, conf);
        ParallelSolution* ps = (ParallelSolution*)sol;
        auto placement = stripe->getPlacement();
        placement.erase(std::remove(placement.begin(), placement.end(), fnid), placement.end());
        ps->_num_agents = conf->_agentsIPs.size();
        ps->genColoringForSingleFailure(stripe, coloring, fnid, conf->_agentsIPs.size(), "scatter", placement);
    }

    // 2.3 get decoding list
    vector<ComputeItem*> decodelist = getDecodeList(decdag, coloring);

    // 2.4 prepare decoding bufMap
    vector<int> decleaves = decdag->getECLeaves();
    unordered_map<int, char*> decBufMap;
    tofree.clear();
    for (int i=0; i<ecn; i++) {
        if (i == fnid) {
            for (int j=0; j<ecw; j++) {
                int dagidx = i*ecw+j;
                char* buf = repairbuffer+j*pktbytes; 
                decBufMap.insert(make_pair(dagidx, buf));
            }
        } else {
            for (int j=0; j<ecw; j++) {
                int dagidx = i*ecw+j;
                char* buf = buffers[i] + j*pktbytes;
                decBufMap.insert(make_pair(dagidx, buf));
            }
        }
    }
    if (method == "offline") {
        // for offline, we put all the leave vertices with color -1 as zero in decBufMap
        for (auto dagidx: ecleaves) {
            if (decBufMap.find(dagidx) == decBufMap.end()) {
                char* buf = (char*)calloc(pktbytes, sizeof(char));
                memset(buf, 0, pktbytes);
                decBufMap.insert(make_pair(dagidx, buf));
                tofree.push_back(dagidx);
            }
        }
    }

    // 1.3 perform decoding
    for (int i=0; i<decodelist.size(); i++) {
        ComputeItem* ci = decodelist[i];
        vector<int> srclist = ci->_srclist;
        int dst = ci->_dstidx;
        vector<int> coefs = ci->_coefs;
        pair<bool, bool> usage = ci->_usage; 

        cout << "dst: " << dst << ", srclist: " ;
        for (auto srcdagidx: srclist)
           cout << srcdagidx << " ";
        cout << endl;

        char** data = (char**)calloc(srclist.size(), sizeof(char*));
        char** code = (char**)calloc(1, sizeof(char*));
        for (int bufIdx = 0; bufIdx < srclist.size(); bufIdx++) {
            int child = srclist[bufIdx];
            int mycolor = coloring[child];

            if (decBufMap.find(child) != decBufMap.end()) {
                data[bufIdx] = decBufMap[child];
            } else {
                //cout << "we should not arrive here for offline method!" << endl;
                assert(coloring.find(child) != coloring.end());
                assert(coloring[child] == -1);

                char* buf = (char*)calloc(pktbytes, sizeof(char));
                memset(buf, 0, pktbytes);
                decBufMap.insert(make_pair(child, buf));
                data[bufIdx] = buf;
            }
        }
        if (decBufMap.find(dst) != decBufMap.end())
            code[0] = decBufMap[dst];
        else {
            code[0] = (char*)calloc(pktbytes, sizeof(char));
            memset(code[0], 0, pktbytes);
            decBufMap.insert(make_pair(dst, code[0]));
            tofree.push_back(dst);
        }

        int* matrix = (int*)calloc(srclist.size(), sizeof(int));
        for (int i=0; i<coefs.size(); i++) {
            matrix[i] = coefs[i];
        }

        Computation::Multi(code, data, matrix, 1, srclist.size(), pktbytes, "Isal");
        //cout << "    value: " << (int)code[0][0] << endl;

        free(matrix);
        free(code);
        free(data);
    }


    // 3. compare decoding result
    bool flag = true;
    for (int i=0; i<blkbytes; i++) {
        if (repairbuffer[i] != buffers[fnid][i]) {
            flag = false;
            break;
        }
    }

    if (flag)
        cout << "SUCCESS!" << endl;
    else
        cout << "FAIL!" << endl;

    return 0;
}