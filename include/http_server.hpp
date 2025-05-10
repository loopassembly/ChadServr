#pragma once

#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>
#include "video_processor.hpp"

namespace chad {

struct HttpRequest {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    std::unordered_map<std::string, std::string> queryParams;
};

struct HttpResponse {
    int statusCode = 200;
    std::string statusText = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    void setJson(const nlohmann::json& jsonObj) {
        body = jsonObj.dump();
        headers["Content-Type"] = "application/json";
    }

    void setText(const std::string& text) {
        body = text;
        headers["Content-Type"] = "text/plain";
    }
};

class HttpServer {
public:
    using RequestHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

    explicit HttpServer(unsigned short port);

    ~HttpServer();

    bool start();

    void stop();

    void setVideoProcessor(std::shared_ptr<VideoProcessor> processor);

    void addRoute(const std::string& method, const std::string& path, RequestHandler handler);

    bool isRunning() const;

private:
    void acceptConnection();

    void handleClient(boost::asio::ip::tcp::socket socket);

    HttpRequest parseRequest(const std::string& requestStr);

    void sendResponse(boost::asio::ip::tcp::socket& socket, const HttpResponse& response);

    RequestHandler findHandler(const std::string& method, const std::string& path);

    std::unordered_map<std::string, std::string> parseQueryParams(const std::string& queryString);

private:
    unsigned short port_;
    std::unique_ptr<boost::asio::io_service> ioService_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::shared_ptr<VideoProcessor> videoProcessor_;

    std::unordered_map<std::string, std::unordered_map<std::string, RequestHandler>> routes_;

    bool running_ = false;
    std::unique_ptr<std::thread> serverThread_;
};

} // namespace chad