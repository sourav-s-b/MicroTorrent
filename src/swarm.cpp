#include "swarm.hpp"
#include "asio/io_context.hpp"
#include "logger.hpp"
#include "peer.hpp"
#include "torrent.hpp"
#include "utils.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

SwarmManager::SwarmManager(const TorrentFile &torrent,
                           const std::vector<PeerData> &peers,
                           uint32_t num_of_connection)
    : torrent_(torrent), disk_(torrent_.name), pieces_downloaded_(0),
      num_of_connection_(num_of_connection) {

  total_pieces_ = (torrent_.total_length + torrent_.piece_length - 1) /
                  torrent_.piece_length; // cheeky technique

  piece_checklist_.assign(total_pieces_, false);

  for (const auto &peer_data : peers) {
    ManagedPeer mp;
    mp.data = peer_data;
    mp.state = PeerState::UNTOUCHED;
    mp.retry_count = 0;
    peer_pool_.push_back(mp);
  }

  Logger::info("Swarm Manager Initialized");
  Logger::info("Target File Size: " + std::to_string(torrent_.total_length) +
               " bytes | " +
               "Available Peers: " + std::to_string(peers.size()) +
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

  Logger::info("Workers dispatched. Entering event loop.");

  maintain_swarm_strength();

  io_context_.run();

  if (is_download_complete()) {
    Logger::info("DOWNLOAD COMPLETE.");
  } else {
    Logger::error(
        "SWARM DEPLETED: Event loop exited, but download is stuck at " +
        std::to_string(pieces_downloaded_) + " / " +
        std::to_string(total_pieces_) + " pieces.");
  }
}

void SwarmManager::request_work(std::shared_ptr<PeerClient> worker) {
  if (is_download_complete()) {
    io_context_.stop();
    return;
  }

  int piece_to_download = get_next_missing_piece(*worker);

  if (piece_to_download != -1) {
    piece_checklist_[piece_to_download] = true;

    uint32_t current_piece_length = torrent_.piece_length;
    if (piece_to_download == total_pieces_ - 1) { // special for the last piece
      current_piece_length = torrent_.total_length % torrent_.piece_length;
      if (current_piece_length == 0)
        current_piece_length = torrent_.piece_length;
    }

    Logger::debug("Delegating Piece " + std::to_string(piece_to_download) +
                  " to Peer." + worker->get_ip());

    worker->fetch_piece_async(piece_to_download, current_piece_length);
  }
}

void SwarmManager::submit_piece(std::shared_ptr<PeerClient> worker,
                                uint32_t piece_index,
                                const std::vector<uint8_t> &data) {
  std::string expected_hash = torrent_.get_hash_for_piece(piece_index);
  std::string raw_data_string(data.begin(), data.end());

  if (SHA1::hash(raw_data_string) == expected_hash) {
    Logger::debug("Hash Verfication Matches! and writing piece " +
                  std::to_string(piece_index));

    disk_.write_piece(piece_index, data.size(), data);
    pieces_downloaded_++;

    Logger::progress("Swarm Progress: " + std::to_string(pieces_downloaded_) +
                         " / " + std::to_string(total_pieces_),
                     LogLevel::INFO);

    request_work(worker);
  } else {
    Logger::error("Hash mismatch! Dropping peer. " + worker->get_ip());
    piece_checklist_[piece_index] = false;
    worker->disconnect();
  }
}

void SwarmManager::handle_disconnect(std::shared_ptr<PeerClient> worker) {
  uint32_t active = worker->get_active_piece();
  if (active != -1) {
    Logger::debug("Peer dropped " + worker->get_ip() + " while holding Piece " +
                  std::to_string(active) + ". Re-queueing.");
    piece_checklist_[active] = false;
  }

  active_workers_.erase(
      std::remove(active_workers_.begin(), active_workers_.end(), worker),
      active_workers_.end());

  for (auto &managed : peer_pool_) {
    if (managed.data.ip == worker->get_ip()) {
      managed.retry_count++;

      if (managed.retry_count >= 5) { //<- retry count.. remeber this is here
        managed.state = PeerState::DEAD;
        Logger::info("[Pool] Peer " + managed.data.ip +
                      " reached retry limit. Marked as DEAD.");
      } else {
        managed.state = PeerState::RETRYING;
        Logger::info("[Pool] Peer " + managed.data.ip +
                      " failed. Re-queued (Attempt " +
                      std::to_string(managed.retry_count) + "/5).");
      }
      break;
    }
  }
  maintain_swarm_strength();
}

void SwarmManager::maintain_swarm_strength() {
  if (is_download_complete())
    return;

  int slots_available = num_of_connection_ - active_workers_.size();

  if (slots_available <= 0)
    return;

  for (auto &managed : peer_pool_) {
    if (slots_available == 0)
      break;

    if (managed.state == PeerState::UNTOUCHED ||
        managed.state == PeerState::RETRYING) {
      managed.state = PeerState::CONNECTING;

      auto worker = std::make_shared<PeerClient>(
          io_context_, managed.data.ip, managed.data.port, torrent_.info_hash,
          "-MT0001-174094882455", *this);

      active_workers_.push_back(worker);
      worker->start();
      slots_available--;
    }
  }
}

void SwarmManager::requeue_piece(int piece_index) {
  if (piece_index != -1) {
    piece_checklist_[piece_index] = false;
    Logger::debug("Piece " + std::to_string(piece_index) +
                  " requeued to swarm.");
  }
}
