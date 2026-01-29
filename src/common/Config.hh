#ifndef _CONFIG_HH_
#define _CONFIG_HH_

#include "../inc/include.hh"
#include "../util/tinyxml2.h"

using namespace tinyxml2;

class Config {
  public:
    Config(std::string& filePath);
    ~Config();
 
    //ip
    unsigned int _localIp;
    std::vector<unsigned int> _agentsIPs;
    std::vector<unsigned int> _repairIPs;
    std::unordered_map<unsigned int, int> _ip2agentid;
    unsigned int _coorIp;

    // size
    int _blkBytes;
    int _pktBytes;

    int _sendGroupSize;
    int _recvGroupSize;
    int _computeGroupSize;

    // code
    std::string _codeName;
    int _ecn;
    int _eck;
    int _ecw;

    // batch size, only for centralize and offline
    int _batch_size;
    int _standby_size;
    int _agents_num;
    
    //worker thread num
    int _agWorkerThreadNum;

    //coor thread num
    int _coorThreadNum;

    //cmddistributor thread num
    int _distThreadNum;

    //path
    std::string _blkDir;
    std::string _ssDir;
    std::string _tpDir;
    
    // network bandwidth
    int _netbdwt;

};
#endif
