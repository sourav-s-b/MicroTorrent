#include "dashboard.hpp"
#include "swarm.hpp"
#include "torrent.hpp"
#include "tracker.hpp"
#include "bencode.hpp"
#include "logger.hpp"
#include <iostream>
#include <exception>
#include <thread>

void run_engine(EngineState& state);

int main() {
    EngineState state;

    Logger::current_level = LogLevel::INFO;

    std::thread engine_thread(run_engine, std::ref(state));
    engine_thread.detach();

    Dashboard dashboard(state);
    dashboard.run();

    return 0;
}

void run_engine(EngineState& state) {
    try {
        std::string torrent_path = "debian-13.5.0-amd64-netinst.iso.torrent";
        try {

          TorrentFile torrent(torrent_path);

          Logger::info("Initializing Tracker Connection...");

          std::string response = TrackerClient::announce(
              torrent.announce_url, torrent.info_hash, torrent.total_length);

          size_t header_end = response.find("\r\n\r\n");
          if (header_end == std::string::npos) {
            throw std::runtime_error(
                "Invalid Tracker Response: Missing HTTP body delimiter.");
          }

          std::string body = response.substr(header_end + 4);
          Logger::debug("Response Arrived");
          size_t parse_index = 0;
          BencodeNode tracker_node = parse_bencode(body, parse_index);
          BencodeDict tracker_dict = std::get<BencodeDict>(tracker_node.data);

          std::string peers_binary =
              std::get<std::string>(tracker_dict.at("peers").data);

          Logger::info("Successfully extracted " +
                       std::to_string(peers_binary.length() / 6) +
                       " peers from the tracker.");

          std::vector<PeerData> peer_list;
          for (size_t i = 0; i < peers_binary.length(); i += 6) {
            uint8_t ip1 = peers_binary[i];
            uint8_t ip2 = peers_binary[i + 1];
            uint8_t ip3 = peers_binary[i + 2];
            uint8_t ip4 = peers_binary[i + 3];

            PeerData pd;
            pd.ip = std::to_string(ip1) + "." + std::to_string(ip2) + "." +
                    std::to_string(ip3) + "." + std::to_string(ip4);

            pd.port = (static_cast<uint8_t>(peers_binary[i + 4]) << 8) |
                      static_cast<uint8_t>(peers_binary[i + 5]);

            peer_list.push_back(pd);
          }

          SwarmManager swarm(torrent, peer_list, 50);
          swarm.start_download();
        } catch (const std::exception &e) {
          Logger::error("Fatal Error: " + std::string(e.what()));
        }
    } catch(std::exception& e) {

    }
}
