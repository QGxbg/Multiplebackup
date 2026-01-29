#ifndef _COMMON_HH_
#define _COMMON_HH_

#include <algorithm>
#include <chrono>
#include <deque>
#include <fstream>
#include <iostream>
#include <mutex>
#include <set>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

#include <cassert>
#include <cmath>
#include <cstring>
#include <ctime>

#include <arpa/inet.h>
#include <hiredis/hiredis.h>

#include <stdio.h>
#include <fcntl.h>

#include <dirent.h>
#include <unistd.h>

#include <iomanip>
#include "../util/Logger.hh"

//#define MAX_COMMAND_LEN 5120
// #define MAX_COMMAND_LEN 10240
#define MAX_COMMAND_LEN 1024
#define AGENT_COMMAND_LEN 128 
#define COMPUTE_COMMAND_LEN 128
#define TASK_COMMAND_LEN 1024
// #define MAX_COMMAND_LEN 102400
#define LOG Logger::getInstance().logFile


#endif

