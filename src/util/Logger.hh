#ifndef _LOGGER_HH_
#define _LOGGER_HH_
#include <iostream>
#include <string>
#include <fstream>
#include <mutex>

class Logger {
public:
    // 单例模式

    // 静态get_instance
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    template<typename T>
    Logger& operator<<(const T& value) {
        std::lock_guard<std::mutex> lock(mutex_);
        logFile << to_string(value);
        return *this;
    }

    Logger& operator<<(const char* value) {
        std::lock_guard<std::mutex> lock(mutex_);
        logFile << value;
        return *this;
    }


    std::ofstream logFile;

private:
    std::mutex mutex_;
    Logger() {
        logFile.open("log.txt", std::ios_base::app);
    }

    ~Logger() {
        logFile.close();
    }

    // 防止复制和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    
};

#endif