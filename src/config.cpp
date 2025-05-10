#include \"../include/config.hpp\"

#include <fstream>
#include <iostream>

namespace chad {

    Config& Config::getInstance() {
        static Config instance;
        return instance;
    }

    bool Config::loadFromFile(const std::string& configPath) {
        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            std::cerr << \"[Config Error] Could not open config file: \" << configPath << std::endl;
            return false;
        }

        try {
            configFile >> config_;
            return true;
        } catch (const nlohmann::json::exception& e) {
            std::cerr << \"[Config Error] Failed to parse config JSON: \" << e.what() << std::endl;
            return false;
        }
    }

    std::string Config::getString(const std::string& key, const std::string& defaultValue) const {
        std::lock_guard<std::mutex> lock(configMutex_);
        if (config_.contains(key) && config_[key].is_string()) {
            return config_[key].get<std::string>();
        }
        return defaultValue;
    }

    int Config::getInt(const std::string& key, int defaultValue) const {
        std::lock_guard<std::mutex> lock(configMutex_);
        if (config_.contains(key) && config_[key].is_number()) {
            return config_[key].get<int>();
        }
        return defaultValue;
    }

    bool Config::getBool(const std::string& key, bool defaultValue) const {
        std::lock_guard<std::mutex> lock(configMutex_);
        if (config_.contains(key) && config_[key].is_boolean()) {
            return config_[key].get<bool>();
        }
        return defaultValue;
    }

} // namespace chad