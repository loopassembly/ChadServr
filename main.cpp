#include <iostream>
#include <memory>
#include <string>
#include <csignal>
#include "include/logger.hpp"
#include "include/config.hpp"
#include "include/http_server.hpp"
#include "include/video_processor.hpp"
#include "include/storage_manager.hpp"
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>

namespace fs = std::filesystem;
using json = nlohmann::json;

std::unique_ptr<chad::HttpServer> g_server;
std::shared_ptr<chad::VideoProcessor> g_videoProcessor;

void signalHandler(int signum) {
    LOG_INFO("Signal received: " + std::to_string(signum));

    if (g_server) {
        LOG_INFO("Shutting down server...");
        g_server->stop();
    }

    std::exit(0);
}

void setupRoutes(chad::HttpServer& server, std::shared_ptr<chad::VideoProcessor> processor) {
    server.addRoute("GET", "/api/status", [&](const chad::HttpRequest& req, chad::HttpResponse& res) {
        json response = {
            {"status", "running"},
            {"version", "1.0.0"},
            {"processor_load", processor->getLoadFactor()},
            {"thread_pool_size", chad::Config::getInstance().getInt("video_processing.thread_pool_size", 4)}
        };
        res.setJson(response);
    });

    server.addRoute("GET", "/api/chunks", [processor](const chad::HttpRequest& req, chad::HttpResponse& res) {
        auto chunks = processor->listChunks();

        json response = json::array();
        for (const auto& chunk : chunks) {
            response.push_back({
                {"id", chunk->chunkId},
                {"status", static_cast<int>(chunk->status)},
                {"size", chunk->size},
                {"width", chunk->width},
                {"height", chunk->height},
                {"duration", chunk->duration},
                {"codec", chunk->codec}
            });
        }

        res.setJson(response);
    });

    server.addRoute("GET", "/api/chunks/info", [processor](const chad::HttpRequest& req, chad::HttpResponse& res) {
        auto params = req.queryParams;
        if (params.find("id") == params.end()) {
            res.statusCode = 400;
            res.statusText = "Bad Request";
            res.setJson({{"error", "Missing chunk id"}});
            return;
        }

        std::string chunkId = params["id"];
        auto chunkInfo = processor->getChunkInfo(chunkId);

        if (!chunkInfo) {
            res.statusCode = 404;
            res.statusText = "Not Found";
            res.setJson({{"error", "Chunk not found"}});
            return;
        }

        json response = {
            {"id", chunkInfo->chunkId},
            {"status", static_cast<int>(chunkInfo->status)},
            {"size", chunkInfo->size},
            {"width", chunkInfo->width},
            {"height", chunkInfo->height},
            {"duration", chunkInfo->duration},
            {"codec", chunkInfo->codec}
        };

        if (!chunkInfo->errorMessage.empty()) {
            response["error"] = chunkInfo->errorMessage;
        }

        res.setJson(response);
    });

    server.addRoute("POST", "/api/upload", [processor](const chad::HttpRequest& req, chad::HttpResponse& res) {
        auto contentType = req.headers.find("Content-Type");
        if (contentType == req.headers.end() || contentType->second.find("video/") != 0) {
            res.statusCode = 400;
            res.statusText = "Bad Request";
            res.setJson({{"error", "Content-Type must be a video format"}});
            return;
        }

        std::string tempPath = fs::path(chad::Config::getInstance().getString("video_processing.temp_path", "storage/temp")).string();

        if (!fs::exists(tempPath)) {
            fs::create_directories(tempPath);
        }

        std::string tempFile = tempPath + "/upload_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".mp4";

        std::ofstream outFile(tempFile, std::ios::binary);
        if (!outFile) {
            res.statusCode = 500;
            res.statusText = "Internal Server Error";
            res.setJson({{"error", "Failed to create temporary file"}});
            return;
        }

        outFile.write(req.body.data(), req.body.size());
        outFile.close();

        json options = json::object();
        auto optionsParam = req.queryParams.find("options");
        if (optionsParam != req.queryParams.end()) {
            try {
                options = json::parse(optionsParam->second);
            } catch (...) {
                LOG_WARNING("Failed to parse options: " + optionsParam->second);
            }
        }

        auto future = processor->processChunk(tempFile, options.dump());
        auto result = future.get();

        res.setJson({
            {"id", result.chunkId},
            {"status", static_cast<int>(result.status)}
        });
    });

    server.addRoute("DELETE", "/api/chunks", [processor](const chad::HttpRequest& req, chad::HttpResponse& res) {
        auto params = req.queryParams;
        if (params.find("id") == params.end()) {
            res.statusCode = 400;
            res.statusText = "Bad Request";
            res.setJson({{"error", "Missing chunk id"}});
            return;
        }

        std::string chunkId = params["id"];
        bool success = processor->deleteChunk(chunkId);

        if (success) {
            res.setJson({{"success", true}});
        } else {
            res.statusCode = 404;
            res.statusText = "Not Found";
            res.setJson({{"error", "Chunk not found or could not be deleted"}});
        }
    });
}

int main() {
    try {
        std::signal(SIGINT, signalHandler);
        std::signal(SIGTERM, signalHandler);

        fs::create_directories("logs");
        chad::Logger::getInstance().initialize("logs/server.log", chad::LogLevel::INFO);
        LOG_INFO("ChadServr starting up");

        if (!fs::exists("config")) {
            fs::create_directories("config");
        }

        if (!fs::exists("config/server_config.json")) {
            LOG_WARNING("Configuration file not found, using defaults");
        } else {
            if (!chad::Config::getInstance().loadFromFile("config/server_config.json")) {
                LOG_ERROR("Failed to load configuration, using defaults");
            } else {
                LOG_INFO("Configuration loaded successfully");
            }
        }

        std::string storagePath = chad::Config::getInstance().getString("video_processing.storage_path", "storage/processed");
        std::string tempPath = chad::Config::getInstance().getString("video_processing.temp_path", "storage/temp");

        for (const auto& path : {storagePath, tempPath}) {
            if (!fs::exists(path)) {
                fs::create_directories(path);
            }
        }

        if (!chad::StorageManager::getInstance().initialize(storagePath)) {
            LOG_ERROR("Failed to initialize storage manager");
            return 1;
        }

        int threadPoolSize = chad::Config::getInstance().getInt("video_processing.thread_pool_size", 2);
        g_videoProcessor = std::make_shared<chad::VideoProcessor>(threadPoolSize);

        if (!g_videoProcessor->initialize(storagePath, tempPath)) {
            LOG_ERROR("Failed to initialize video processor");
            return 1;
        }

        g_videoProcessor->setMaxChunks(chad::Config::getInstance().getInt("video_processing.max_chunks", 100));

        int port = chad::Config::getInstance().getInt("server.port", 8080);
        g_server = std::make_unique<chad::HttpServer>(port);
        g_server->setVideoProcessor(g_videoProcessor);

        setupRoutes(*g_server, g_videoProcessor);

        if (!g_server->start()) {
            LOG_ERROR("Failed to start server");
            return 1;
        }

        LOG_INFO("Server started on port " + std::to_string(port));

        while (g_server->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        LOG_INFO("Server stopped normally");
        return 0;
    } catch (const std::exception& e) {
        LOG_FATAL("Unhandled exception: " + std::string(e.what()));
        return 1;
    }
}