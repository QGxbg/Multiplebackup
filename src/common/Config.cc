#include "Config.hh"

Config::Config(std::string &filepath) {
  XMLDocument doc;
  doc.LoadFile(filepath.c_str());
  XMLElement *element;
  for (element =
           doc.FirstChildElement("setting")->FirstChildElement("attribute");
       element != NULL; element = element->NextSiblingElement("attribute")) {
    XMLElement *ele = element->FirstChildElement("name");
    std::string attName = ele->GetText();
    if (attName == "controller.addr") {
      _coorIp = inet_addr(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "agents.addr") {
      for (ele = ele->NextSiblingElement("value"); ele != NULL;
           ele = ele->NextSiblingElement("value")) {
        std::string rack = "default";
        std::string ipstr = ele->GetText();
        unsigned int ip = inet_addr(ipstr.c_str());
        _agentsIPs.push_back(ip);
      }
      for (int i = 0; i < _agentsIPs.size(); i++) {
        unsigned int ip = _agentsIPs[i];
        _ip2agentid.insert(std::make_pair(ip, i));
      }
    } else if (attName == "repairnodes.addr") {
      for (ele = ele->NextSiblingElement("value"); ele != NULL;
           ele = ele->NextSiblingElement("value")) {
        std::string rack = "default";
        std::string ipstr = ele->GetText();
        unsigned int ip = inet_addr(ipstr.c_str());
        _repairIPs.push_back(ip);
      }
    } else if (attName == "controller.thread.num") {
      _coorThreadNum = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "agent.thread.num") {
      _agWorkerThreadNum =
          std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "block.bytes") {
      _blkBytes = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "packet.bytes") {
      _pktBytes = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "code.name") {
      _codeName = ele->NextSiblingElement("value")->GetText();
    } else if (attName == "code.ecn") {
      _ecn = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "code.eck") {
      _eck = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "code.ecw") {
      _ecw = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "batch.size") {
      _batch_size = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "batch.method") {
      _batchmethod = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "sendgroup.size") {
      _sendGroupSize = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "recvgroup.size") {
      _recvGroupSize = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "computegroup.size") {
      _computeGroupSize =
          std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "cmddist.thread.num") {
      _distThreadNum = std::stoi(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "local.addr") {
      _localIp = inet_addr(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "block.directory") {
      _blkDir = std::string(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "stripestore.directory") {
      _ssDir = std::string(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "tradeoffpoint.directory") {
      _tpDir = std::string(ele->NextSiblingElement("value")->GetText());
    } else if (attName == "network.bandwidth") {
      _netbdwt = std::stoi(ele->NextSiblingElement("value")->GetText());
    }
  }
  _agents_num = _agentsIPs.size();
  _standby_size = _repairIPs.size();
}

Config::~Config() {}
