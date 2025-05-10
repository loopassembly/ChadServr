#include "../include/http_server.hpp"
#include "../include/logger.hpp"
#include <iostream>
#include <sstream>
#include <thread>
#include <regex>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

namespace chad {

using boost::asio::ip::tcp;

HttpServer::HttpServer(unsigned short port) : port_(port), running_(false) {
    ioService_ = std::make_unique<boost::asio::io_service>();
}

HttpServer::~HttpServer() {
    stop();
}

bool HttpServer::start() {
    if (running_) {
        LOG_WARNING("Server is already running");
        return false;
    }
    
    try {
        acceptor_ = std::make_unique<tcp::acceptor>(*ioService_, tcp::endpoint(tcp::v4(), port_));
        running_ = true;
        
        // Start accepting connections in a separate thread
        serverThread_ = std::make_unique<std::thread>([this]() {
            LOG_INFO("Server started on port " + std::to_string(port_));
            
            try {
                while (running_) {
                    acceptConnection();
                }
            } catch (const std::exception& e) {
                if (running_) { // Only log if not stopping deliberately
                    LOG_ERROR("Server thread exception: " + std::string(e.what()));
                }
            }
            
            LOG_INFO("Server thread stopped");
        });
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to start server: " + std::string(e.what()));
        running_ = false;
        return false;
    }
}

void HttpServer::stop() {
    if (!running_) {
        return;
    }
    
    LOG_INFO("Stopping server");
    running_ = false;
    
    // Close acceptor to stop accepting new connections
    if (acceptor_ && acceptor_->is_open()) {
        boost::system::error_code ec;
        acceptor_->close(ec);
    }
    
    // Stop IO service
    if (ioService_) {
        ioService_->stop();
    }
    
    // Wait for server thread to finish
    if (serverThread_ && serverThread_->joinable()) {
        serverThread_->join();
    }
    
    LOG_INFO("Server stopped");
}

void HttpServer::setVideoProcessor(std::shared_ptr<VideoProcessor> processor) {
    videoProcessor_ = processor;
}

void HttpServer::addRoute(const std::string& method, const std::string& path, RequestHandler handler) {
    routes_[method][path] = handler;
    LOG_DEBUG("Added route: " + method + " " + path);
}

bool HttpServer::isRunning() const {
    return running_;
}

void HttpServer::acceptConnection() {
    tcp::socket socket(*ioService_);
    acceptor_->accept(socket);
    
    // Handle client in a separate thread
    std::thread([this, sock = std::move(socket)]() mutable {
        handleClient(std::move(sock));
    }).detach();
}

void HttpServer::handleClient(tcp::socket socket) {
    try {
        boost::asio::streambuf buffer;
        boost::system::error_code error;
        
        // Read the request
        boost::asio::read_until(socket, buffer, "\r\n\r\n", error);
        
        if (error) {
            LOG_ERROR("Error reading request: " + error.message());
            return;
        }
        
        // Convert to string
        std::string requestStr(
            boost::asio::buffers_begin(buffer.data()),
            boost::asio::buffers_begin(buffer.data()) + buffer.size()
        );
        
        // Parse the request
        HttpRequest request = parseRequest(requestStr);
        
        // Read any remaining body data for POST/PUT requests
        if (request.method == "POST" || request.method == "PUT") {
            auto contentLengthIt = request.headers.find("Content-Length");
            if (contentLengthIt != request.headers.end()) {
                size_t contentLength = std::stoul(contentLengthIt->second);
                
                // If we've already read part of the body
                size_t headerSize = requestStr.find("\r\n\r\n") + 4;
                size_t bodyAlreadyRead = requestStr.size() - headerSize;
                
                // If there's more to read
                if (bodyAlreadyRead < contentLength) {
                    size_t remaining = contentLength - bodyAlreadyRead;
                    std::vector<char> bodyBuffer(remaining);
                    
                    size_t bytesRead = boost::asio::read(
                        socket, 
                        boost::asio::buffer(bodyBuffer),
                        boost::asio::transfer_exactly(remaining),
                        error
                    );
                    
                    if (error && error != boost::asio::error::eof) {
                        LOG_ERROR("Error reading request body: " + error.message());
                    } else {
                        request.body.append(requestStr.substr(headerSize));
                        request.body.append(bodyBuffer.begin(), bodyBuffer.begin() + bytesRead);
                    }
                } else {
                    request.body = requestStr.substr(headerSize);
                }
            }
        }
        
        // Find and execute the appropriate handler
        RequestHandler handler = findHandler(request.method, request.path);
        
        HttpResponse response;
        if (handler) {
            // Execute the handler
            try {
                handler(request, response);
            } catch (const std::exception& e) {
                LOG_ERROR("Exception in request handler: " + std::string(e.what()));
                response.statusCode = 500;
                response.statusText = "Internal Server Error";
                response.setText("Internal server error: " + std::string(e.what()));
            }
        } else {
            // No handler found, return 404
            response.statusCode = 404;
            response.statusText = "Not Found";
            response.setText("Resource not found: " + request.path);
        }
        
        // Send the response
        sendResponse(socket, response);
    } catch (const std::exception& e) {
        LOG_ERROR("Exception handling client: " + std::string(e.what()));
    }
}

HttpRequest HttpServer::parseRequest(const std::string& requestStr) {
    HttpRequest request;
    std::istringstream requestStream(requestStr);
    
    // Parse the request line
    std::string requestLine;
    std::getline(requestStream, requestLine);
    
    // Remove carriage return if present
    if (!requestLine.empty() && requestLine.back() == '\r') {
        requestLine.pop_back();
    }
    
    // Parse method, path (with query string), and HTTP version
    std::istringstream lineStream(requestLine);
    lineStream >> request.method >> request.path >> request.version;
    
    // Parse query parameters if present
    size_t queryPos = request.path.find('?');
    if (queryPos != std::string::npos) {
        std::string queryString = request.path.substr(queryPos + 1);
        request.path = request.path.substr(0, queryPos);
        request.queryParams = parseQueryParams(queryString);
    }
    
    // Parse headers
    std::string headerLine;
    while (std::getline(requestStream, headerLine) && headerLine != "\r" && headerLine != "") {
        if (headerLine.back() == '\r') {
            headerLine.pop_back();
        }
        
        size_t colonPos = headerLine.find(':');
        if (colonPos != std::string::npos) {
            std::string name = headerLine.substr(0, colonPos);
            std::string value = headerLine.substr(colonPos + 1);
            
            // Trim leading whitespace from value
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            
            request.headers[name] = value;
        }
    }
    
    return request;
}

std::unordered_map<std::string, std::string> HttpServer::parseQueryParams(const std::string& queryString) {
    std::unordered_map<std::string, std::string> params;
    std::istringstream stream(queryString);
    std::string param;
    
    while (std::getline(stream, param, '&')) {
        size_t equalsPos = param.find('=');
        if (equalsPos != std::string::npos) {
            std::string name = param.substr(0, equalsPos);
            std::string value = param.substr(equalsPos + 1);
            params[name] = value;
        } else {
            // Parameter with no value
            params[param] = "";
        }
    }
    
    return params;
}

void HttpServer::sendResponse(tcp::socket& socket, const HttpResponse& response) {
    std::stringstream ss;
    
    // Status line
    ss << "HTTP/1.1 " << response.statusCode << " " << response.statusText << "\r\n";
    
    // Content length
    ss << "Content-Length: " << response.body.size() << "\r\n";
    
    // Headers
    for (const auto& header : response.headers) {
        ss << header.first << ": " << header.second << "\r\n";
    }
    
    // End of headers
    ss << "\r\n";
    
    // Body
    ss << response.body;
    
    // Send the response
    std::string responseStr = ss.str();
    boost::asio::write(socket, boost::asio::buffer(responseStr));
}

HttpServer::RequestHandler HttpServer::findHandler(const std::string& method, const std::string& path) {
    // Try exact match first
    auto methodIt = routes_.find(method);
    if (methodIt != routes_.end()) {
        auto pathIt = methodIt->second.find(path);
        if (pathIt != methodIt->second.end()) {
            return pathIt->second;
        }
    }
    
    // TODO: Implement pattern matching for path parameters
    
    return nullptr; // No handler found
}

} // namespace chad