#include "swarm.hpp"
#include "asio/io_context.hpp"
#include "logger.hpp"
#include "peer.hpp"
#include "torrent.hpp"
#include "tracker.hpp"
#include "utils.hpp"
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

SwarmManager::SwarmManager(const TorrentFile &torrent,
                           const std::vector<PeerData> &peers)
    : torrent_(torrent), peers_(peers), pieces_downloaded_(0) {

  total_pieces_ = (torrent_.total_length + torrent_.piece_length - 1) /
                  torrent_.piece_length; // cheeky technique

  piece_checklist_.assign(total_pieces_, false);

  Logger::info("Swarm Manager Initialized");
  Logger::info("Target File Size: " + std::to_string(torrent_.total_length) +
               " bytes | " +
               "Available Peers: " + std::to_string(peers_.size()) +
               " | Total Pieces required: " + std::to_string(total_pieces_));
}

bool SwarmManager::is_download_complete() const {
  return pieces_downloaded_ == total_pieces_;
}

int SwarmManager::get_next_missing_piece(const PeerClient &worker) const {
  for (uint32_t i = 0; i < total_pieces_; ++i) {
    if (!piece_checklist_[i] && worker.has_piece(i)) {
      return i;
    }
  }
  return -1;
}

void SwarmManager::start_download() {
  asio::io_context peer_io_context;
  std::string peer_id = generate_peer_id();

  std::ofstream init_file(torrent_.name, std::ios::binary | std::ios::app);
  init_file.close();

  size_t current_peer_index = 0;

  Logger::info("Starting Swarm Download Sequece------");

  while (!is_download_complete()) {
    if (current_peer_index >= peers_.size()) {
      Logger::error("Reached end of peer list. Cycling back to the beginning");
    }

    PeerData &target = peers_[current_peer_index];
    current_peer_index++;

    PeerClient worker(peer_io_context, target.ip, target.port,
                      torrent_.info_hash, peer_id);

    if (worker.connect_and_handshake()) {

      while (!is_download_complete()) {
        int piece_to_download = get_next_missing_piece(worker);
        std::string expected_hash =
            torrent_.get_hash_for_piece(piece_to_download);
        if (piece_to_download == -1)
          Logger::debug("Peer doesn't have piece next piece");
        break;

        uint32_t current_piece_length = torrent_.piece_length;
        // last piece may be smoll
        if (piece_to_download == total_pieces_ - 1) {
          current_piece_length = torrent_.total_length * torrent_.piece_length;
          if (current_piece_length == 0)
            current_piece_length = torrent_.piece_length;
        }

        std::vector<uint8_t> piece_data =
            worker.download_piece(piece_to_download, torrent_.piece_length);

        if (piece_data.size() == current_piece_length) {
          std::string raw_data_string(piece_data.begin(), piece_data.end());

          if (SHA1::hash(raw_data_string) == expected_hash) {
            Logger::info(
                "Hash Verification Successfull! Writing piece to disk");

            std::fstream out(torrent_.name,
                             std::ios::binary | std::ios::in | std::ios::out);

            out.seekp(piece_to_download * torrent_.piece_length);
            out.write(reinterpret_cast<const char *>(piece_data.data()),
                      piece_data.size());
            out.close();

            piece_checklist_[piece_to_download] = true;
            pieces_downloaded_++;

            Logger::progress(
                "Swarm Progress: " + std::to_string(pieces_downloaded_) +
                    " / " + std::to_string(total_pieces_) +
                    " pieces complete.\n",
                LogLevel::DEBUG);
          } else {
            Logger::error("Hash Verification Failed. Dropping piece");
            break;
          }
        } else {
          Logger::error("Download failed or incomplete or peer stopped "
                        "sending data. Moving to next peer.");
          break;
        }
      }
    }
  }
  Logger::info("Download Complete");
}
