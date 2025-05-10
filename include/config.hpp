#pragma once

#include <string>
#include <unordered_map>
#include <mutex>
#include <nlohmann/json.hpp>

namespace chad {

class Config {
public:
    static Config& getInstance();
    
    bool loadFromFile(const std::string& configPath);
    
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    
    int getInt(const std::string& key, int defaultValue = 0) const;
    
    bool getBool(const std::string& key, bool defaultValue = false) const;
    
    template <typename T>
    void setValue(const std::string& key, const T& value) {
        std::lock_guard<std::mutex> lock(configMutex_);
        config_[key] = value;
    }

private:
    Config() = default;
    ~Config() = default;
    
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    
    nlohmann::json config_;
    mutable std::mutex configMutex_;
};

} // namespace chad