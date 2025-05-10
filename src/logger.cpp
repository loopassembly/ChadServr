#include "../include/logger.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <ctime>

namespace chad {

    Logger& Logger::getInstance() {
        static Logger instance;
        return instance;
    }

    bool Logger::initialize(const std::string& logFilePath, LogLevel minLevel, bool consoleOutput) {
        std::lock_guard<std::mutex> lock(logMutex_);

        if (initialized_) {
            if (logFile_.is_open()) {
                logFile_.close();
            }
        }

        logFile_.open(logFilePath, std::ios::out | std::ios::app);
        if (!logFile_.is_open()) {
            std::cerr << "Logger Error: Failed to open log file: " << logFilePath << std::endl;
            return false;
        }

        minLevel_ = minLevel;
        consoleOutput_ = consoleOutput;
        initialized_ = true;

        log(LogLevel::INFO, "Logger initialized successfully.");
        return true;
    }

    void Logger::log(LogLevel level, const std::string& message) {
        if (level < minLevel_) {
            return;
        }

        std::lock_guard<std::mutex> lock(logMutex_);

        std::string timestamp = getCurrentTimestamp();
        std::string levelStr = levelToString(level);
        std::string formattedMessage = timestamp + " [" + levelStr + "] " + message;

        if (initialized_ && logFile_.is_open()) {
            logFile_ << formattedMessage << std::endl;
            logFile_.flush();
        }

        if (consoleOutput_) {
            if (level == LogLevel::ERROR || level == LogLevel::FATAL) {
                std::cerr << formattedMessage << std::endl;
            } else {
                std::cout << formattedMessage << std::endl;
            }
        }
    }

    void Logger::debug(const std::string& message) {
        log(LogLevel::DEBUG, message);
    }

    void Logger::info(const std::string& message) {
        log(LogLevel::INFO, message);
    }

    void Logger::warning(const std::string& message) {
        log(LogLevel::WARNING, message);
    }

    void Logger::error(const std::string& message) {
        log(LogLevel::ERROR, message);
    }

    void Logger::fatal(const std::string& message) {
        log(LogLevel::FATAL, message);
    }

    void Logger::setLogLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(logMutex_);
        minLevel_ = level;
    }

    void Logger::close() {
        std::lock_guard<std::mutex> lock(logMutex_);
        if (initialized_ && logFile_.is_open()) {
            logFile_.close();
        }
        initialized_ = false;
    }

    Logger::~Logger() {
        close();
    }

    std::string Logger::levelToString(LogLevel level) const {
        switch (level) {
            case LogLevel::DEBUG:   return "DEBUG";
            case LogLevel::INFO:    return "INFO";
            case LogLevel::WARNING: return "WARNING";
            case LogLevel::ERROR:   return "ERROR";
            case LogLevel::FATAL:   return "FATAL";
            default:                return "UNKNOWN";
        }
    }

    std::string Logger::getCurrentTimestamp() const {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;

        std::stringstream ss;
        std::tm tm_buf;
        localtime_r(&time_t_now, &tm_buf);

        ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S")
           << '.' << std::setfill('0') << std::setw(3) << ms.count();

        return ss.str();
    }

} // namespace chad