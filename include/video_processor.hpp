#pragma once

#include <string>
#include <vector>
#include <memory>
#include <future>
#include <functional>
#include <atomic>
#include "thread_pool.hpp"

namespace chad {

/**
 * @enum ProcessingStatus
 * @brief Status of video chunk processing
 */
enum class ProcessingStatus {
    PENDING,
    PROCESSING,
    COMPLETED,
    FAILED
};

/**
 * @struct ChunkInfo
 * @brief Information about a video chunk
 */
struct ChunkInfo {
    std::string chunkId;
    std::string filePath;
    size_t size;
    ProcessingStatus status;
    std::string errorMessage;
    
    // Metadata
    int width = 0;
    int height = 0;
    double duration = 0.0;
    std::string codec;
};

/**
 * @class VideoProcessor
 * @brief Processes video chunks with various operations
 */
class VideoProcessor {
public:
    /**
     * @brief Default constructor
     */
    VideoProcessor();
    
    /**
     * @brief Constructor with thread pool size
     * @param threadPoolSize Number of threads for processing
     */
    explicit VideoProcessor(size_t threadPoolSize);
    
    /**
     * @brief Destructor
     */
    ~VideoProcessor();
    
    /**
     * @brief Initialize the processor
     * @param storagePath Path for storing processed chunks
     * @param tempPath Path for temporary files
     * @return true if initialization was successful
     */
    bool initialize(const std::string& storagePath, const std::string& tempPath);
    
    /**
     * @brief Process a video chunk
     * @param inputPath Path to the input chunk
     * @param options Processing options as JSON string
     * @return ChunkInfo with processing details
     */
    std::future<ChunkInfo> processChunk(const std::string& inputPath, const std::string& options);
    
    /**
     * @brief Get information about a processed chunk
     * @param chunkId ID of the chunk
     * @return ChunkInfo if found, nullptr otherwise
     */
    std::shared_ptr<ChunkInfo> getChunkInfo(const std::string& chunkId) const;
    
    /**
     * @brief List all processed chunks
     * @return Vector of chunk information
     */
    std::vector<std::shared_ptr<ChunkInfo>> listChunks() const;
    
    /**
     * @brief Delete a processed chunk
     * @param chunkId ID of the chunk to delete
     * @return true if deleted successfully
     */
    bool deleteChunk(const std::string& chunkId);
    
    /**
     * @brief Set the maximum number of chunks to keep
     * @param maxChunks Maximum number (0 = unlimited)
     */
    void setMaxChunks(size_t maxChunks);
    
    /**
     * @brief Get the current processing load (0.0-1.0)
     * @return Load factor
     */
    double getLoadFactor() const;

private:
    // Extract metadata from video file
    ChunkInfo extractMetadata(const std::string& filePath);
    
    // Generate a unique chunk ID
    std::string generateChunkId();
    
    // Clean old chunks if max limit reached
    void cleanupOldChunks();

private:
    std::unique_ptr<ThreadPool> threadPool_;
    std::string storagePath_;
    std::string tempPath_;
    size_t maxChunks_;
    
    // Keep track of all chunks
    mutable std::mutex chunksMutex_;
    std::vector<std::shared_ptr<ChunkInfo>> chunks_;
    
    // Statistics
    std::atomic<size_t> processedChunks_;
    std::atomic<size_t> failedChunks_;
};

} // namespace chad