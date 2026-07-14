#pragma once

#include "swarm.hpp"
#include <asio.hpp>
#include <string>
#include <vector>
class TrackerClient {
public:
  std::vector<PeerData> parse_peer_binary(const std::string &peer_binary);

  std::vector<PeerData>
  fetch_peers(const std::string &announce_url,
              const std::vector<std::string> &announce_list,
              const std::string &info_hash, long long total_length);

private:
  bool parse_url(const std::string &url, std::string &protocol,
                 std::string &host, std::string &port, std::string &path);

  std::vector<PeerData> announce(const std::string &announce_url,
                                 const std::string &info_hash,
                                 long long total_length);
  std::vector<PeerData>
  announce_http( const std::string &host,
                const std::string &port, const std::string &path,
                const std::string &info_hash, long long total_length);
  std::vector<PeerData>
  announce_https( const std::string &host,
                 const std::string &port, const std::string &path,
                 const std::string &info_hash, long long total_length);
  std::vector<PeerData>
  announce_udp(const std::string &host,
               const std::string &port, 
               const std::string &info_hash, long long total_length);
};

inline std::string generate_peer_id() {
  std::string id = "-MT0001-";
  while (id.length() < 20) {
    id += std::to_string(rand() % 10);
  }
  return id;
}
