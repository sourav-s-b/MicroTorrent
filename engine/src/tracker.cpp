#include "tracker.hpp"
#include "asio/io_context.hpp"
#include "asio/system_error.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <exception>
#include <iostream>
#include <regex>
#include <sstream>
#include <stdexcept>
bool TrackerClient::parse_url(const std::string &url, std::string &host,
                              std::string &port, std::string &path) {

  std::regex url_regex(R"(^http://([^/:]+)(?::(\d+))?(/.*)$)");
  std::smatch match;

  if (std::regex_match(url, match, url_regex)) {
    host = match[1].str();
    port = match[2].matched ? match[2].str() : "80";
    path = match[3].str();
    return true;
  }

  return false;
}
std::string TrackerClient::announce(const std::string &announce_url,
                                    const std::string &info_hash,
                                    long long total_length) {
  std::string host, port, path;

  if (!parse_url(announce_url, host, port, path)) {
    throw std::runtime_error("Invalid URL or unsupported protocol");
  }

  Logger::info("Initialing Tracker Connection");
  Logger::debug("Target Host: " + host + " | Port: " + port);

  asio::io_context io_context;

  asio::ip::tcp::resolver resolver(io_context);
  asio::ip::tcp::socket socket(io_context);

  std::string response_data;

  try {
    Logger::info("Resolving DNS...");
    auto endpoints = resolver.resolve(host, port);

    Logger::info("Connecting socket...");
    asio::connect(socket, endpoints);

    Logger::info("Connected successfully!");

    std::string peer_id = "-MT0001-174094882455";
    std::string encoded_info_hash = url_encode(info_hash);

    std::ostringstream request_stream;
    request_stream << "GET " << path << "?"
                   << "info_hash=" << encoded_info_hash
                   << "&peer_id=" << peer_id << "&port=6881"
                   << "&uploaded=0"
                   << "&downloaded=0"
                   << "&left=" << total_length << "&compact=1"
                   << " HTTP/1.1\r\n";

    request_stream << "Host: " << host << "\r\n";
    request_stream << "Connection: close\r\n";
    request_stream << "\r\n";

    std::string full_request = request_stream.str();
    Logger::info("Sending HTTP Request");
    Logger::debug(full_request);

    asio::write(socket, asio::buffer(full_request));
    Logger::info("Request transmitted. Awaiting response");

    char read_buffer[4096];
    asio::error_code ec;

    while (true) {
      size_t bytes_transferred =
          socket.read_some(asio::buffer(read_buffer), ec);

      if (bytes_transferred > 0) {
        response_data.append(read_buffer, bytes_transferred);
      }

      if (ec == asio::error::eof) {
        break;
      } else if (ec) {
        throw asio::system_error(ec);
      }
    }
  } catch (std::exception &e) {
    throw std::runtime_error("Network connection failed: " +
                             std::string(e.what()));
  }
  return response_data;
}
