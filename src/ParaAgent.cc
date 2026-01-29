#include "common/Config.hh"
#include "common/Worker.hh"

#include "inc/include.hh"

using namespace std;

int main(int argc, char** argv) {

    string configPath = "conf/sysSetting.xml";
    Config* conf = new Config(configPath);
    
    Worker* worker = new Worker(conf);
    thread t = thread([=]{worker->doProcess();});
    t.join();
    
    delete worker;
    delete conf;

    return 0;
}

