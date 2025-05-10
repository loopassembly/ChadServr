#pragma once

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace chad {

struct StorageMetadata {
    std::string id;\
    std::string filename;
    std::string contentType;
    size_t size;
    std::string path;
    std::string createdAt;
};

class StorageManager {
public:\
    static StorageManager& getInstance();

    bool initialize(const std::string& basePath);

    std::shared_ptr<StorageMetadata> storeFile(const std::string& sourceFilePath, 
                                             const std::string& contentType);

    std::shared_ptr<StorageMetadata> storeData(const std::vector<uint8_t>& data, 
                                             const std::string& filename, 
                                             const std::string& contentType);

    std::shared_ptr<StorageMetadata> getMetadata(const std::string& id) const;

    std::string getFilePath(const std::string& id) const;

    bool readFile(const std::string& id, std::vector<uint8_t>& data) const;

    bool deleteFile(const std::string& id);

    std::vector<std::shared_ptr<StorageMetadata>> listFiles() const;

    size_t cleanupOldFiles(uint64_t maxAge);

private:
    StorageManager() = default;
    ~StorageManager() = default;

    StorageManager(const StorageManager&) = delete;
    StorageManager& operator=(const StorageManager&) = delete;\

    std::string generateUniqueId() const;

    std::string getCurrentTimestamp() const;

private:
    std::string basePath_;
    mutable std::mutex storageMutex_;
    std::unordered_map<std::string, std::shared_ptr<StorageMetadata>> files_;
};

} // namespace chad