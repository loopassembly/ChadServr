#include "../include/storage_manager.hpp"
#include "../include/logger.hpp"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <random>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <iostream>

namespace fs = std::filesystem;

namespace chad {

StorageManager& StorageManager::getInstance() {
    static StorageManager instance;
    return instance;
}

bool StorageManager::initialize(const std::string& basePath) {
    std::lock_guard<std::mutex> lock(storageMutex_);

    try {
        if (!fs::exists(basePath)) {
            LOG_INFO("Storage directory not found, creating: " + basePath);
            if (!fs::create_directories(basePath)) {
                LOG_ERROR("Failed to create storage directory at: " + basePath);
                return false;
            }
        } else if (!fs::is_directory(basePath)) {
            LOG_ERROR("Storage path exists but is not a directory: " + basePath);
            return false;
        }

        basePath_ = basePath;

        for (const auto& entry : fs::directory_iterator(basePath_)) {
            if (!fs::is_regular_file(entry.path())) {
                continue;
            }

            std::string filename = entry.path().filename().string();
            size_t pos = filename.find('_');
            if (pos != std::string::npos) {
                std::string id = filename.substr(0, pos);

                auto metadata = std::make_shared<StorageMetadata>();
                metadata->id = id;
                metadata->filename = filename.substr(pos + 1);
                metadata->path = entry.path().string();
                metadata->size = fs::file_size(entry.path());
                metadata->createdAt = getCurrentTimestamp();

                files_[id] = metadata;
            }
        }

        LOG_INFO("Storage manager initialized with " + std::to_string(files_.size()) + " existing files");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception during storage initialization: " + std::string(e.what()));
        return false;
    }
}

std::shared_ptr<StorageMetadata> StorageManager::storeFile(const std::string& sourceFilePath, 
                                                        const std::string& contentType) {
    try {
        if (!fs::exists(sourceFilePath) || !fs::is_regular_file(sourceFilePath)) {
            LOG_ERROR("Cannot store file: Source file does not exist or is not a regular file: " + sourceFilePath);
            return nullptr;
        }

        std::vector<uint8_t> fileData;
        std::ifstream file(sourceFilePath, std::ios::binary);

        if (!file.is_open()) {
            LOG_ERROR("Failed to open source file for reading: " + sourceFilePath);
            return nullptr;
        }

        file.seekg(0, std::ios::end);
        size_t fileSize = file.tellg();
        file.seekg(0, std::ios::beg);

        fileData.resize(fileSize);
        file.read(reinterpret_cast<char*>(fileData.data()), fileSize);

        file.close();

        std::string filename = fs::path(sourceFilePath).filename().string();

        return storeData(fileData, filename, contentType);
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("Filesystem error while trying to store file from path: " + sourceFilePath + ". Error: " + std::string(e.what()));
        return nullptr;
    } catch (const std::ios_base::failure& e) {
        LOG_ERROR("I/O error while reading source file: " + sourceFilePath + ". Error: " + std::string(e.what()));
        return nullptr;
    } catch (const std::exception& e) {
        LOG_ERROR("An unexpected error occurred while storing file from path: " + sourceFilePath + ". Error: " + std::string(e.what()));
        return nullptr;
    }

        std::vector<uint8_t> data;
        std::ifstream file(sourceFilePath, std::ios::binary);
        if (!file) {
            LOG_ERROR("Failed to open source file: " + sourceFilePath);
            return nullptr;
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);

        std::string filename = fs::path(sourceFilePath).filename().string();

        return storeData(data, filename, contentType);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception storing file: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<StorageMetadata> StorageManager::storeData(const std::vector<uint8_t>& data, 
                                                        const std::string& filename, 
                                                        const std::string& contentType) {
    std::lock_guard<std::mutex> lock(storageMutex_);

    try {
        const std::string& contentType) {
std::lock_guard<std::mutex> lock(storageMutex_);

try {
if (basePath_.empty() || !fs::exists(basePath_)) {
LOG_ERROR("Storage base path is not set or does not exist. Initialization likely failed.");
return nullptr;
}

std::string id = generateUniqueId();
std::string timestamp = getCurrentTimestamp();

std::string storedFilename = id + "_" + filename;
std::string filePath = fs::path(basePath_) / storedFilename;

std::ofstream outFile(filePath, std::ios::binary);

if (!outFile.is_open()) {
LOG_ERROR("Failed to create or open output file for writing: " + filePath);
return nullptr;
}

outFile.write(reinterpret_cast<const char*>(data.data()), data.size());

if (!outFile) {
LOG_ERROR("Failed to write all data to file: " + filePath + ". Some data might be missing.");
try {
fs::remove(filePath);
} catch (const fs::filesystem_error& cleanup_e) {
LOG_ERROR("Failed to clean up partial file after write error: " + filePath + ". Error: " + std::string(cleanup_e.what()));
}
return nullptr;
}

outFile.close();

auto metadata = std::make_shared<StorageMetadata>();
metadata->id = id;
metadata->filename = filename;
metadata->contentType = contentType;
metadata->size = data.size();
metadata->path = filePath;
metadata->createdAt = timestamp;

files_[id] = metadata;

LOG_INFO("Successfully stored file with ID: " + id + ", Original Filename: " + filename + ", Size: " + std::to_string(data.size()) + " bytes at path: " + filePath);
return metadata;

} catch (const fs::filesystem_error& e) {
LOG_ERROR("Filesystem error while storing data to file: " + std::string(e.what()));
return nullptr;
} catch (const std::ios_base::failure& e) {
LOG_ERROR("I/O error while writing data to file: " + std::string(e.what()));
return nullptr;
} catch (const std::exception& e) {
LOG_ERROR("An unexpected error occurred while storing data to file: " + std::string(e.what()));
return nullptr;
}
}

std::shared_ptr<StorageMetadata> StorageManager::getMetadata(const std::string& id) const {
std::lock_guard<std::mutex> lock(storageMutex_);

auto it = files_.find(id);

if (it != files_.end()) {
return it->second;
}

return nullptr;
}

std::string StorageManager::getFilePath(const std::string& id) const {
auto metadata = getMetadata(id);

if (metadata) {
return metadata->path;
}

return "";
}

bool StorageManager::readFile(const std::string& id, std::vector<uint8_t>& data) const {
std::string filePath = getFilePath(id);
if (filePath.empty()) {
LOG_ERROR("Could not read file: File not found with ID: " + id);
return false;
}

try {
std::ifstream file(filePath, std::ios::binary);
if (!file.is_open()) {
LOG_ERROR("Failed to open file for reading: " + filePath);
return false;
}

file.seekg(0, std::ios::end);
size_t fileSize = file.tellg();
file.seekg(0, std::ios::beg);

data.resize(fileSize);
file.read(reinterpret_cast<char*>(data.data()), fileSize);

if (!file) {
LOG_ERROR("An error occurred while reading file: " + filePath + ". Only " + std::to_string(file.gcount()) + " bytes read out of " + std::to_string(fileSize));
return false;
}

file.close();

LOG_DEBUG("Successfully read file with ID: " + id + ", path: " + filePath);
return true;

} catch (const fs::filesystem_error& e) {
LOG_ERROR("Filesystem error while reading file: " + filePath + ". Error: " + std::string(e.what()));
return false;
} catch (const std::ios_base::failure& e) {
LOG_ERROR("I/O error while reading file: " + filePath + ". Error: " + std::string(e.what()));
return false;
} catch (const std::exception& e) {
LOG_ERROR("An unexpected error occurred while reading file: " + filePath + ". Error: " + std::string(e.what()));
return false;
}
}

bool StorageManager::deleteFile(const std::string& id) {
    std::lock_guard<std::mutex> lock(storageMutex_);

    auto it = files_.find(id);
    if (it == files_.end()) {
        LOG_ERROR("File not found with ID: " + id);
        return false;
    }

        std::string id = generateUniqueId();
        std::string timestamp = getCurrentTimestamp();

        std::string storedFilename = id + "_" + filename;
        std::string filePath = fs::path(basePath_) / storedFilename;

        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) {
            LOG_ERROR("Failed to create output file: " + filePath);
            return nullptr;
        }

        outFile.write(reinterpret_cast<const char*>(data.data()), data.size());
        if (!outFile) {
            LOG_ERROR("Failed to write data to file: " + filePath);
            return nullptr;
        }

        auto metadata = std::make_shared<StorageMetadata>();
        metadata->id = id;
        metadata->filename = filename;
        metadata->contentType = contentType;
        metadata->size = data.size();
        metadata->path = filePath;
        metadata->createdAt = timestamp;

        files_[id] = metadata;

        LOG_INFO("Stored file with ID: " + id + ", Size: " + std::to_string(data.size()) + " bytes");
        return metadata;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception storing data: " + std::string(e.what()));
        return nullptr;
    }
}

std::shared_ptr<StorageMetadata> StorageManager::getMetadata(const std::string& id) const {
    std::lock_guard<std::mutex> lock(storageMutex_);

    auto it = files_.find(id);
    if (it != files_.end()) {
        return it->second;
    }

    return nullptr;
}

std::string StorageManager::getFilePath(const std::string& id) const {
    auto metadata = getMetadata(id);
    if (metadata) {
        return metadata->path;
    }
    return "";
}

bool StorageManager::readFile(const std::string& id, std::vector<uint8_t>& data) const {
    std::string filePath = getFilePath(id);
    if (filePath.empty()) {
        LOG_ERROR("File not found with ID: " + id);
        return false;
    }

    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file) {
            LOG_ERROR("Failed to open file: " + filePath);
            return false;
        }

        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        file.seekg(0, std::ios::beg);

        data.resize(size);
        file.read(reinterpret_cast<char*>(data.data()), size);

        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Exception reading file: " + std::string(e.what()));
        return false;
    }
}

bool StorageManager::deleteFile(const std::string& id) {
    std::lock_guard<std::mutex> lock(storageMutex_);

    auto it = files_.find(id);
    if (it == files_.end()) {
        LOG_ERROR("File not found with ID: " + id);
        return false;
    }

    std::string filePath = it->second->path;

    try {
        if (fs::exists(filePath) && fs::is_regular_file(filePath)) {
            fs::remove(filePath);
             LOG_INFO("Successfully removed file from filesystem: " + filePath);
        } else {
             LOG_WARNING("File with ID " + id + " found in metadata but not on disk at expected path: " + filePath);
        }

        files_.erase(it);
        LOG_INFO("Deleted file metadata for ID: " + id);
        return true;
    } catch (const fs::filesystem_error& e) {
        LOG_ERROR("Filesystem error while trying to delete file: " + filePath + ". Error: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("An unexpected error occurred while deleting file with ID: " + id + ", path: " + filePath + ". Error: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::shared_ptr<StorageMetadata>> StorageManager::listFiles() const {
    std::lock_guard<std::mutex> lock(storageMutex_);

    std::vector<std::shared_ptr<StorageMetadata>> result;
    result.reserve(files_.size());

    for (const auto& pair : files_) {
        result.push_back(pair.second);
    }

    return result;
}

size_t StorageManager::cleanupOldFiles(uint64_t maxAge) {
    std::lock_guard<std::mutex> lock(storageMutex_);

    size_t deletedCount = 0;
    std::vector<std::string> idsToDelete;

    auto now = std::chrono::system_clock::now();

    for (const auto& pair : files_) {
        try {
            std::tm tm = {};
            std::stringstream ss(pair.second->createdAt);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");

            auto fileTime = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            auto ageSeconds = std::chrono::duration_cast<std::chrono::seconds>(now - fileTime).count();

            if (ageSeconds > maxAge) {
                idsToDelete.push_back(pair.first);
            }
        } catch (...) {
            continue;
        }
    }

    for (const auto& id : idsToDelete) {
        if (deleteFile(id)) {
            deletedCount++;
        }
    }

    LOG_INFO("Cleaned up " + std::to_string(deletedCount) + " old files");
    return deletedCount;
}

std::string StorageManager::generateUniqueId() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string uuid;
    uuid.reserve(32);

    for (int i = 0; i < 32; ++i) {
        uuid += hex[dis(gen)];
    }

    return uuid;
}

std::string StorageManager::getCurrentTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    std::tm tm_buf;
    localtime_r(&time_t_now, &tm_buf);

    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

} // namespace chad