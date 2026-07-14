#include "tracker.hpp"
#include "asio/buffer.hpp"
#include "asio/impl/write.hpp"
#include "asio/io_context.hpp"
#include "asio/system_error.hpp"
#include "bencode.hpp"
#include "logger.hpp"
#include "swarm.hpp"
#include "utils.hpp"
#include <asio/ssl.hpp>
#include <cstdint>
#include <exception>
#include <iostream>
#include <openssl/ssl.h>
#include <random>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <winsock2.h>

void write_u32(std::vector<uint8_t> &buf, uint32_t val) {
  buf.push_back((val >> 24) & 0xFF);
  buf.push_back((val >> 16) & 0xFF);
  buf.push_back((val >> 8) & 0xFF);
  buf.push_back(val & 0xFF);
}

void write_u64(std::vector<uint8_t> &buf, uint64_t val) {
  write_u32(buf, (val >> 32) & 0xFFFFFFFF);
  write_u32(buf, val & 0xFFFFFFFF);
}

uint32_t read_u32(const uint8_t *buf) {
  return (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
}

uint64_t read_u64(const uint8_t *buf) {
  return (static_cast<uint64_t>(read_u32(buf)) << 32) | read_u32(buf + 4);
}

bool TrackerClient::parse_url(const std::string &url, std::string &protocol,
                              std::string &host, std::string &port,
                              std::string &path) {
  std::regex url_regex(R"(^(http|https|udp)://([^/:]+)(?::(\d+))?(/.*)?$)");
  std::smatch match;

  if (std::regex_match(url, match, url_regex)) {
    protocol = match[1].str();
    host = match[2].str();

    if (match[3].matched) {
      port = match[3].str();
    } else if (protocol == "https") {
      port = "443";
    } else {
      port = "80";
    }

    path = match[4].matched ? match[4].str() : "/";
    return true;
  }

  return false;
}

std::vector<PeerData>
TrackerClient::fetch_peers(const std::string &announce_url,
                           const std::vector<std::string> &announce_list,
                           const std::string &info_hash,
                           long long total_length) {
  std::vector<std::string> trackers_list;
  if (!announce_url.empty()) {
    trackers_list.push_back(announce_url);
  }

  for (const auto &url : announce_list) {
    if (url != announce_url) {
      trackers_list.push_back(url);
    }
  }

  if (trackers_list.empty()) {
    throw std::runtime_error("No Valid announce url found");
  }

  for (const std::string &url : trackers_list) {
    try {
      Logger::debug("Trying tracker: " + url);
      std::vector<PeerData> peers = announce(url, info_hash, total_length);

      if (!peers.empty()) {
        Logger::debug("Successfully fetched " + std::to_string(peers.size()) +
                      " peers.");
        return peers;
      }
    } catch (const std::exception &e) {
      Logger::debug("Tracker " + url + " failed: " + e.what());
    }
  }

  throw std::runtime_error("All trackers failed to return peers.");
}

std::vector<PeerData> TrackerClient::announce(const std::string &announce_url,
                                              const std::string &info_hash,
                                              long long total_length) {
  std::string protocol, host, port, path;

  if (!parse_url(announce_url, protocol, host, port, path)) {
    throw std::runtime_error("Invalid URL: " + announce_url);
  }

  if (protocol == "http") {
    return announce_http(host, port, path, info_hash, total_length);
  }

  throw std::runtime_error("Protocol Couldn't be matched");
}

std::vector<PeerData>
TrackerClient::parse_peer_binary(const std::string &peers_binary) {
  std::vector<PeerData> peer_list;
  if (peers_binary.length() % 6 != 0) {
    Logger::error("Warning: Peer binary string length is not a multiple of 6.");
  }
  for (size_t i = 0; i < peers_binary.length(); i += 6) {

    uint8_t ip1 = static_cast<uint8_t>(peers_binary[i]);
    uint8_t ip2 = static_cast<uint8_t>(peers_binary[i + 1]);
    uint8_t ip3 = static_cast<uint8_t>(peers_binary[i + 2]);
    uint8_t ip4 = static_cast<uint8_t>(peers_binary[i + 3]);

    PeerData pd;
    pd.ip = std::to_string(ip1) + "." + std::to_string(ip2) + "." +
            std::to_string(ip3) + "." + std::to_string(ip4);

    uint8_t port_high = static_cast<uint8_t>(peers_binary[i + 4]);
    uint8_t port_low = static_cast<uint8_t>(peers_binary[i + 5]);
    pd.port = (port_high << 8) | port_low;

    peer_list.push_back(pd);
  }
  return peer_list;
}

std::vector<PeerData> TrackerClient::announce_http(const std::string &host,
                                                   const std::string &port,
                                                   const std::string &path,
                                                   const std::string &info_hash,
                                                   long long total_length) {
  Logger::info("Initialing Tracker Connection");
  Logger::debug("Target Host: " + host + " | Port: " + port,
                LogChannel::TRACKER);

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
    Logger::info("Sending HTTP Request", LogChannel::TRACKER);
    Logger::debug(full_request);

    asio::write(socket, asio::buffer(full_request));
    Logger::info("Request transmitted. Awaiting response", LogChannel::TRACKER);

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

    size_t header_end = response_data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      throw std::runtime_error(
          "Invalid Tracker Response: Missing HTTP body delimiter.");
    }

    std::string body = response_data.substr(header_end + 4);
    Logger::debug("Response Arrived");

    size_t parse_index = 0;
    BencodeNode tracker_node = parse_bencode(body, parse_index);
    BencodeDict tracker_dict = std::get<BencodeDict>(tracker_node.data);

    if (tracker_dict.count("failure reason")) {
      throw std::runtime_error(
          "Tracker error: " +
          std::get<std::string>(tracker_dict.at("failure reason").data));
    }

    std::string peers_binary =
        std::get<std::string>(tracker_dict.at("peers").data);

    Logger::info("Successfully extracted " +
                 std::to_string(peers_binary.length() / 6) +
                 " peers from the tracker.");

    return parse_peer_binary(peers_binary);

  } catch (std::exception &e) {
    throw std::runtime_error("Network connection failed: " +
                             std::string(e.what()));
  }
}

std::vector<PeerData> TrackerClient::announce_udp(const std::string &host,
                                                  const std::string &port,
                                                  const std::string &info_hash,
                                                  long long total_length) {

  Logger::debug("Target Host: " + host + " | Port: " + port,
                LogChannel::TRACKER);

  asio::io_context io_context;
  asio::ip::udp::resolver resolver(io_context);
  auto endpoints = resolver.resolve(host, port);

  asio::ip::udp::socket socket(io_context);
  socket.open(asio::ip::udp::v4());

  struct timeval tv;
  DWORD timeout_ms = 5000;
  int result = setsockopt(socket.native_handle(), SOL_SOCKET, SO_RCVTIMEO,
                          reinterpret_cast<const char *>(&timeout_ms),
                          sizeof(timeout_ms));

  std::random_device rd;
  std::mt19937 gen(rd());
  uint32_t transaction_id = gen();

  // connect request
  std::vector<uint8_t> connect_req;
  write_u64(connect_req, 0x41727101980);
  write_u32(connect_req, 0);
  write_u32(connect_req, transaction_id);

  auto endpoint = *endpoints.begin();
  socket.send_to(asio::buffer(connect_req), endpoint);

  // the response
  uint8_t recv_buf[2048];
  asio::ip::udp::endpoint sender_endpoint;
  size_t len = socket.receive_from(asio::buffer(recv_buf), sender_endpoint);

  if (len < 16)
    throw std::runtime_error("UDP Connect response too short");

  uint32_t action = read_u32(recv_buf);
  if (action == 3)
    throw std::runtime_error("Tracker returned error");
  if (action != 0 || read_u32(recv_buf + 4) != transaction_id) {
    throw std::runtime_error("Invalid connect response");
  }
  uint64_t connection_id = read_u64(recv_buf + 8);

  // announce request
  transaction_id = gen();
  std::vector<uint8_t> announce_req;
  write_u64(announce_req, connection_id);
  write_u32(announce_req, 1);
  write_u32(announce_req, transaction_id);

  for (int i = 0; i < 20; ++i)
    announce_req.push_back(info_hash[i]);

  std::string peer_id = "-MT0001-174094882455";
  for (int i = 0; i < 20; ++i)
    announce_req.push_back(peer_id[i]);

  write_u64(announce_req, 0);            // download
  write_u64(announce_req, total_length); // left
  write_u64(announce_req, 0);            // uploaded
  write_u32(announce_req, 0);            // event
  write_u32(announce_req, 0);            // ip
  write_u32(announce_req, gen());        // key
  write_u32(announce_req, -1);           // num_want

  announce_req.push_back(0x1A); // port 6881 high byte
  announce_req.push_back(0xE1); // port 6881 low byte

  socket.send_to(asio::buffer(announce_req), endpoint);

  // announce response
  len = socket.receive_from(asio::buffer(recv_buf), sender_endpoint);

  if (len < 20)
    throw std::runtime_error("UDP Announce response too short");

  action = read_u32(recv_buf);
  if (action == 3)
    throw std::runtime_error("Tracker returned error");
  if (action != 1 || read_u32(recv_buf + 4) != transaction_id) {
    throw std::runtime_error("Invalid announce response");
  }

  std::string peers_binary(reinterpret_cast<char *>(recv_buf + 20), len - 20);

  Logger::info("Successfully extracted " +
               std::to_string(peers_binary.length() / 6) +
               " peers from the tracker.");

  return parse_peer_binary(peers_binary);
}

std::vector<PeerData> TrackerClient::announce_https(
    const std::string &host, const std::string &port, const std::string &path,
    const std::string &info_hash, long long total_length) {
  Logger::info("Initialing Tracker Connection");
  Logger::debug("Target Host: " + host + " | Port: " + port,
                LogChannel::TRACKER);

  asio::io_context io_context;

  asio::ssl::context ctx(asio::ssl::context::tlsv12_client);
  ctx.set_default_verify_paths();

  asio::ssl::stream<asio::ip::tcp::socket> socket(io_context, ctx);

  if (!SSL_set_tlsext_host_name(socket.native_handle(), host.c_str())) {
    throw std::runtime_error("Failed to set SNI hostname");
  }

  asio::ip::tcp::resolver resolver(io_context);
  std::string response_data;

  try {
    Logger::info("Resolving DNS...");
    auto endpoints = resolver.resolve(host, port);

    Logger::info("Connecting socket...");
    asio::connect(socket.lowest_layer(), endpoints);
    socket.handshake(asio::ssl::stream_base::client);

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
    Logger::info("Sending HTTP Request", LogChannel::TRACKER);
    Logger::debug(full_request);

    asio::write(socket, asio::buffer(full_request));
    Logger::info("Request transmitted. Awaiting response", LogChannel::TRACKER);

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

    size_t header_end = response_data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      throw std::runtime_error(
          "Invalid Tracker Response: Missing HTTP body delimiter.");
    }

    std::string body = response_data.substr(header_end + 4);
    Logger::debug("Response Arrived");

    size_t parse_index = 0;
    BencodeNode tracker_node = parse_bencode(body, parse_index);
    BencodeDict tracker_dict = std::get<BencodeDict>(tracker_node.data);

    if (tracker_dict.count("failure reason")) {
      throw std::runtime_error(
          "Tracker error: " +
          std::get<std::string>(tracker_dict.at("failure reason").data));
    }

    std::string peers_binary =
        std::get<std::string>(tracker_dict.at("peers").data);

    Logger::info("Successfully extracted " +
                 std::to_string(peers_binary.length() / 6) +
                 " peers from the tracker.");

    return parse_peer_binary(peers_binary);

  } catch (std::exception &e) {
    throw std::runtime_error("Network connection failed: " +
                             std::string(e.what()));
  }
}
