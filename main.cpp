#include <iostream>
#include <sstream>
#include <boost/asio.hpp>
#include <nlohmann/json.hpp>

using namespace boost::asio;
using json = nlohmann::json;

void handleRequest(const std::string& path, ip::tcp::socket& socket) {
    json jsonResponse;

    if (path == "/") {
        jsonResponse["message"] = "Hello, World!";
    } else if (path == "/about") {
        jsonResponse["message"] = "About Page Content";
    } else {
        jsonResponse["error"] = "404 Not Found";
    }

    std::string response = "HTTP/1.1 200 OK\r\nContent-Length: " + std::to_string(jsonResponse.dump().size()) + "\r\n\r\n" + jsonResponse.dump();
    boost::asio::write(socket, boost::asio::buffer(response));
}

int main() {
    io_service io;
    ip::tcp::acceptor acceptor(io, ip::tcp::endpoint(ip::tcp::v4(), 1234));

    while (true) {
        ip::tcp::socket socket(io);
        acceptor.accept(socket);

        std::string request;
        boost::asio::read_until(socket, boost::asio::dynamic_buffer(request), '\n');

        std::istringstream iss(request);
        std::string method, path, http_version;
        iss >> method >> path >> http_version;

        handleRequest(path, socket);
    }

    return 0;
}
