#include "../include/video_processor.hpp"
#include "../include/logger.hpp"
#include "../include/storage_manager.hpp"
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <random>
#include <sstream>
#include <iomanip>
#include <cstdio>
#include <memory>
#include <array>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace chad {

std::string execCommand(const std::string& cmd) {
    std::array<char, 128> buffer;
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);

    if (!pipe) {
        throw std::runtime_error("popen() failed");
    }

    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }

    return result;
}

VideoProcessor::VideoProcessor() : VideoProcessor(std::thread::hardware_concurrency()) {}

VideoProcessor::VideoProcessor(size_t threadPoolSize) 
    : maxChunks_(0), processedChunks_(0), failedChunks_(0) {
    threadPool_ = std::make_unique<ThreadPool>(threadPoolSize > 0 ? threadPoolSize : std::thread::hardware_concurrency());
    LOG_INFO("Video processor created with thread pool size: " + std::to_string(threadPool_->getActiveThreadCount()));
}

VideoProcessor::~VideoProcessor() {
    LOG_INFO("Video processor shutting down");
}

bool VideoProcessor::initialize(const std::string& storagePath, const std::string& tempPath) {
    try {
        for (const auto& path : {storagePath, tempPath}) {
            if (!fs::exists(path)) {
                if (!fs::create_directories(path)) {
                    LOG_ERROR("Failed to create directory: " + path);
                    return false;
                }
            }
        }

        storagePath_ = storagePath;
        tempPath_ = tempPath;

        try {
            std::string ffmpegVersion = execCommand("ffmpeg -version");
            if (ffmpegVersion.empty()) {
                LOG_WARNING("FFmpeg check returned empty result");
            } else {
                LOG_INFO("FFmpeg detected: " + ffmpegVersion.substr(0, ffmpegVersion.find('\n')));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("FFmpeg check failed: " + std::string(e.what()));
            return false;
        }

        LOG_INFO("Video processor initialized");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize video processor: " + std::string(e.what()));
        return false;
    }
}

std::future<ChunkInfo> VideoProcessor::processChunk(const std::string& inputPath, const std::string& optionsStr) {
    return threadPool_->submit([this, inputPath, optionsStr]() -> ChunkInfo {
        ChunkInfo info;
        info.chunkId = generateChunkId();
        info.filePath = inputPath;
        info.status = ProcessingStatus::PROCESSING;

        LOG_INFO("Processing chunk " + info.chunkId + " from " + inputPath);

        try {
            json options;
            if (!optionsStr.empty()) {
                options = json::parse(optionsStr);
            }

            info.size = fs::file_size(inputPath);

            if (!fs::exists(inputPath)) {
                throw std::runtime_error("Input file does not exist");
            }

            std::string outputFilename = info.chunkId + "_processed.mp4";
            std::string outputPath = fs::path(storagePath_) / outputFilename;

            info = extractMetadata(inputPath);
            info.chunkId = info.chunkId.empty() ? generateChunkId() : info.chunkId;
            info.status = ProcessingStatus::PROCESSING;

            std::string ffmpegCmd = "ffmpeg -y -i \"" + inputPath + "\"";

            if (options.contains("resize")) {
                if (options["resize"].contains("width") && options["resize"].contains("height")) {
                    int width = options["resize"]["width"];
                    int height = options["resize"]["height"];
                    ffmpegCmd += " -vf scale=" + std::to_string(width) + ":" + std::to_string(height);
                }
            }

            if (options.contains("bitrate")) {
                std::string bitrate = options["bitrate"];
                ffmpegCmd += " -b:v " + bitrate;
            }

            if (options.contains("codec")) {
                std::string codec = options["codec"];
                ffmpegCmd += " -c:v " + codec;
            }

            ffmpegCmd += " \"" + outputPath + "\" 2>&1";

            std::string output = execCommand(ffmpegCmd);
            LOG_DEBUG("FFmpeg output: " + output);

            if (!fs::exists(outputPath)) {
                throw std::runtime_error("Processing failed, output file not created");
            }

            info.filePath = outputPath;
            info.size = fs::file_size(outputPath);
            info.status = ProcessingStatus::COMPLETED;

            {
                std::lock_guard<std::mutex> lock(chunksMutex_);
                chunks_.push_back(std::make_shared<ChunkInfo>(info));
                processedChunks_++;

                cleanupOldChunks();
            }

            LOG_INFO("Finished processing chunk " + info.chunkId);

        } catch (const std::exception& e) {
            LOG_ERROR("Error processing chunk: " + std::string(e.what()));
            info.status = ProcessingStatus::FAILED;
            info.errorMessage = e.what();

            {
                std::lock_guard<std::mutex> lock(chunksMutex_);
                chunks_.push_back(std::make_shared<ChunkInfo>(info));
                failedChunks_++;
            }
        }

        return info;
    });
}

std::shared_ptr<ChunkInfo> VideoProcessor::getChunkInfo(const std::string& chunkId) const {
    std::lock_guard<std::mutex> lock(chunksMutex_);

    auto it = std::find_if(chunks_.begin(), chunks_.end(), [&chunkId](const std::shared_ptr<ChunkInfo>& chunk) {
        return chunk->chunkId == chunkId;
    });

    if (it != chunks_.end()) {
        return *it;
    }

    return nullptr;
}

std::vector<std::shared_ptr<ChunkInfo>> VideoProcessor::listChunks() const {
    std::lock_guard<std::mutex> lock(chunksMutex_);
    return chunks_;
}

bool VideoProcessor::deleteChunk(const std::string& chunkId) {
    std::lock_guard<std::mutex> lock(chunksMutex_);

    auto it = std::find_if(chunks_.begin(), chunks_.end(), [&chunkId](const std::shared_ptr<ChunkInfo>& chunk) {
        return chunk->chunkId == chunkId;
    });

    if (it != chunks_.end()) {
        try {
            if (fs::exists((*it)->filePath)) {
                fs::remove((*it)->filePath);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("Error deleting chunk file: " + std::string(e.what()));
            return false;
        }

        chunks_.erase(it);
        return true;
    }

    return false;
}

void VideoProcessor::setMaxChunks(size_t maxChunks) {
    maxChunks_ = maxChunks;

    if (maxChunks_ > 0) {
        cleanupOldChunks();
    }
}

double VideoProcessor::getLoadFactor() const {
    size_t activeThreads = threadPool_->getActiveThreadCount();
    size_t queueSize = threadPool_->getQueueSize();
    size_t totalCapacity = threadPool_->getActiveThreadCount() * 2;

    double loadFactor = static_cast<double>(activeThreads + queueSize) / totalCapacity;
    return std::min(1.0, loadFactor);
}

ChunkInfo VideoProcessor::extractMetadata(const std::string& filePath) {
    ChunkInfo info;
    info.filePath = filePath;

    try {
        std::string ffprobeCmd = "ffprobe -v error -select_streams v:0 -show_entries "
                              "stream=width,height,codec_name,duration -of json \"" + 
                              filePath + "\" 2>&1";

        std::string output = execCommand(ffprobeCmd);

        json metadata = json::parse(output);

        if (metadata.contains("streams") && !metadata["streams"].empty()) {
            auto& stream = metadata["streams"][0];

            if (stream.contains("width")) {
                info.width = stream["width"];
            }

            if (stream.contains("height")) {
                info.height = stream["height"];
            }

            if (stream.contains("codec_name")) {
                info.codec = stream["codec_name"];
            }

            if (stream.contains("duration")) {
                info.duration = std::stod(stream["duration"].get<std::string>());
            }
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Error extracting metadata: " + std::string(e.what()));
    }

    return info;
}

std::string VideoProcessor::generateChunkId() {
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

void VideoProcessor::cleanupOldChunks() {
    if (maxChunks_ > 0 && chunks_.size() > maxChunks_) {
        std::sort(chunks_.begin(), chunks_.end(), [](const std::shared_ptr<ChunkInfo>& a, const std::shared_ptr<ChunkInfo>& b) {
            return a.get() < b.get();
        });

        size_t toDelete = chunks_.size() - maxChunks_;

        for (size_t i = 0; i < toDelete && !chunks_.empty(); ++i) {
            auto& chunk = chunks_[0];

            try {
                if (fs::exists(chunk->filePath)) {
                    fs::remove(chunk->filePath);
                }
                LOG_INFO("Auto-deleted old chunk: " + chunk->chunkId);
            } catch (const std::exception& e) {
                LOG_ERROR("Error during auto-cleanup: " + std::string(e.what()));
            }

            chunks_.erase(chunks_.begin());
        }
    }
}

} // namespace chad