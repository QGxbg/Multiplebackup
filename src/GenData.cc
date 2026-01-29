#include "inc/include.hh"
#include "util/DistUtil.hh"
#include "common/Config.hh"
#include "common/Stripe.hh"
#include "ec/ECBase.hh"
#include "ec/Clay.hh"
#include "sol/SolutionBase.hh"
#include "sol/OfflineSolution.hh"
#include "sol/ParallelSolution.hh"

using namespace std;

void usage() {
    cout << "Usage: ./GenData" << endl;
    cout << "    1. code" << endl;
    cout << "    2. n" << endl;
    cout << "    3. k" << endl;
    cout << "    4. w" << endl;
    cout << "    5. blockMiB" << endl;
    cout << "    6. subpktKiB" << endl;
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

int main(int argc, char** argv) {
    if (argc != 7) {
        usage();
        return 0;
    }

    string code = argv[1];
    int ecn = atoi(argv[2]);
    int eck = atoi(argv[3]);
    int ecw = atoi(argv[4]);
    int blkMB = atoi(argv[5]);
    int subpktKB = atoi(argv[6]);

    int blkbytes = blkMB * 1048576;
    int subpktbytes = subpktKB * 1024;
    int pktbytes = subpktbytes * ecw;

    string configpath="./conf/sysSetting.xml";
    Config* conf = new Config(configpath);
    string blkdir = "./blkDir";

    string stripename = code+"-"+to_string(ecn)+to_string(eck)+to_string(ecw);
    vector<string> blklist;
    for (int i=0; i<ecn; i++) {
        string blkname = stripename+"-"+to_string(i);
        blklist.push_back(blkname);
    }

    ECBase* ec;
    if (code == "Clay") {
        ec = new Clay(ecn,eck,ecw,{to_string(ecn-1)});
    } else {
        cout << "Non-supported code!" << endl;
    }

    int stripenum = blkbytes / pktbytes;
    int slicebytes = pktbytes / ecw;
    int pktnum = blkbytes/pktbytes;

    // 0. prepare buffers
    char** buffers = (char**)calloc(ecn, sizeof(char*));
    for (int i=0; i<ecn; i++) {
        buffers[i] = (char*)calloc(pktbytes, sizeof(char));
        memset(buffers[i], 0, pktbytes);
        if (i < eck) {
            for (int j=0; j<ecw; j++) {
                int cid = i*ecw+j;
                memset(buffers[i] + subpktbytes * j, cid, subpktbytes);
            }
        } else {
            memset(buffers[i], 0, pktbytes);
        }
    }

    // 1. encode ecdag
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
            char* buf = buffers[i] + j*subpktbytes;
            encBufMap.insert(make_pair(dagidx, buf));
        }
    }
    for (auto dagidx: ecleaves) {
        if (encBufMap.find(dagidx) == encBufMap.end()) {
            char* buf = (char*)calloc(subpktbytes, sizeof(char));
            memset(buf, 0, subpktbytes);
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
            code[0] = (char*)calloc(subpktbytes, sizeof(char));
            memset(code[0], 0, subpktbytes);
            encBufMap.insert(make_pair(dst, code[0]));
            tofree.push_back(dst);
        }
        int* matrix = (int*)calloc(srclist.size(), sizeof(int));
        for (int i=0; i<coefs.size(); i++) {
            matrix[i] = coefs[i];
        }
        Computation::Multi(code, data, matrix, 1, srclist.size(), subpktbytes, "Isal");
    }

    // 1.4 debug
    for (int i=0; i<ecn; i++) {
        cout << "blk " << i << ": ";
        for (int j=0; j<ecw; j++) {
            char c = *(buffers[i]+subpktbytes*j);
            cout << (int)c << " ";
        }
        cout << endl;
    }

    // write databuffer to blkDir
    for (int i=0; i<ecn; i++) {
        string blkname = blklist[i];
        string fullpath = blkdir + "/" + blkname;
        ofstream ofs(fullpath);
        ofs.close();
        ofs.open(fullpath, ios::app);
        for (int j=0; j<pktnum; j++)
            ofs.write(buffers[i], pktbytes);
        free(buffers[i]);
        ofs.close();
    }

    for (int i=0; i<encodelist.size(); i++) {
        ComputeItem* ci = encodelist[i];
        if (ci)
            delete ci;
    }
}
