#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace chad {

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static Logger& getInstance();
    
    bool initialize(const std::string& logFilePath, LogLevel minLevel = LogLevel::INFO, bool consoleOutput = true);
    
    void log(LogLevel level, const std::string& message);
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    void fatal(const std::string& message);
    
    void setLogLevel(LogLevel level);
    void close();

private:
    Logger() = default;
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string levelToString(LogLevel level) const;
    std::string getCurrentTimestamp() const;
    
    std::ofstream logFile_;
    LogLevel minLevel_ = LogLevel::INFO;
    bool consoleOutput_ = true;
    bool initialized_ = false;
    std::mutex logMutex_;
};

#define LOG_DEBUG(message) chad::Logger::getInstance().debug(message)
#define LOG_INFO(message) chad::Logger::getInstance().info(message)
#define LOG_WARNING(message) chad::Logger::getInstance().warning(message)
#define LOG_ERROR(message) chad::Logger::getInstance().error(message)
#define LOG_FATAL(message) chad::Logger::getInstance().fatal(message)

} // namespace chad