#include "bencode.hpp"
#include "logger.hpp"
#include "swarm.hpp"
#include "torrent.hpp"
#include "tracker.hpp"
#include <stdexcept>
#include <string>
#include <vector>

int main() {
  Logger::current_level = LogLevel::INFO;
  Logger::disable_channel(LogChannel::MESSAGE);
  std::string torrent_path = "debian-13.5.0-amd64-netinst.iso.torrent";

  try {
    TorrentFile torrent(torrent_path);

    Logger::info("Initializing Tracker Connection...");

    // Create the instance and get the vector directly
    TrackerClient tracker;
    std::vector<PeerData> peer_list = tracker.fetch_peers(
        torrent.announce_url, torrent.announce_list,  torrent.info_hash, torrent.total_length);

    Logger::info("Successfully extracted " + std::to_string(peer_list.size()) + " peers from the tracker.");

    SwarmManager swarm(torrent, peer_list, 50);
    swarm.start_download();

  } catch (const std::exception &e) {
    Logger::error("Fatal Error: " + std::string(e.what()));
  }
  return 0;
}
