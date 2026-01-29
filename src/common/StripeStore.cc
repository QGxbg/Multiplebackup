#include "StripeStore.hh"
#include <iostream>

StripeStore::StripeStore(Config* conf) {
    LOG << "StripeStore::StripeStore start" <<endl;
    _conf = conf;
    string sspath = conf->_ssDir + "/placement";
    LOG << sspath << endl;

    // 0. read block placement
    ifstream infile(sspath);
    string line;
    int stripeid=0;

    while (getline(infile, line)) {
        vector<string> items = DistUtil::splitStr(line.substr(0, line.length()-1), " ");
        // vector<string> items = DistUtil::splitStr(line.substr(0, line.length()), " ");
        string stripename = items[0];
        vector<string> blklist;
        vector<unsigned int> iplist;
        vector<int> nodeidlist;
        for (int i=1; i<items.size(); i++) {
            vector<string> tmpitems = DistUtil::splitStr(items[i], ":");
            string blockname = tmpitems[0];
            string locationstr = tmpitems[1];
            unsigned int location = inet_addr(locationstr.c_str());
            int nodeid = _conf->_ip2agentid[location];
            blklist.push_back(blockname);
            iplist.push_back(location);
            nodeidlist.push_back(nodeid);
            cout << blockname << " " << locationstr << " " << nodeid << " " << endl;
        }
        Stripe* curstripe = new Stripe(stripeid++, stripename, blklist, iplist, nodeidlist);
        _stripe_list.push_back(curstripe);
    }

    // LOG << "StripeStore::placement " << _stripe_list.size() << endl;
    // for(auto stripe : _stripe_list){
    //     auto placement = stripe->getPlacement();
    //     for(auto it: placement){
    //         LOG << " " << it;
    //     }
    //     LOG << endl;
    // }
    // LOG << "StripeStore::StripeStore end" <<endl;
}

StripeStore::~StripeStore() {
    // note that we generate stripes inside stripe store
    // thus, we free stripes inside stripe store
    for (int i=0; i<_stripe_list.size(); i++) {
        Stripe* curstripe = _stripe_list[i];
        if (curstripe)
            delete curstripe;
    }
}

vector<Stripe*> StripeStore::getStripeList() {
    return _stripe_list;
}
