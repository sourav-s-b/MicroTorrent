#pragma once

#include "file_manager.hpp"
#include "peer.hpp"
#include "torrent.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

struct PeerData {
  std::string ip;
  uint16_t port;

  friend std::ostream &operator<<(std::ostream &os, const PeerData &peer) {
    return os << peer.ip << ":" << peer.port;
  }

  std::string to_string() const { return ip + ":" + std::to_string(port); }
};

enum class PeerState { UNTOUCHED, CONNECTING, ACTIVE, RETRYING, DEAD };

struct ManagedPeer {
  PeerData data;
  PeerState state = PeerState::UNTOUCHED;
  int retry_count = 0;
};

struct SwarmState {
  std::string torrent_name = "";
  uint32_t total_pieces = 0;
  uint32_t downloaded_pieces = 0;
  uint32_t pool_size = 0;
  uint32_t active_peers = 0;
};

class SwarmManager {
public:
  SwarmManager() = default;
  SwarmManager(const TorrentFile &torrent, const std::vector<PeerData> &peers,
               uint32_t num_of_connection);

  void init(const TorrentFile &torrent, const std::vector<PeerData> &peers,
            uint32_t num_of_connection);

  void start_download();

  void request_work(std::shared_ptr<PeerClient> worker);

  void submit_piece(std::shared_ptr<PeerClient> worker, uint32_t piece_index,
                    const std::vector<uint8_t> &data);

  void handle_disconnect(std::shared_ptr<PeerClient> worker);

  void requeue_piece(int piece_index);

  SwarmState get_stats();

private:
  TorrentFile torrent_;
  std::vector<ManagedPeer> peer_pool_;
  FileManager disk_;
  uint32_t num_of_connection_;

  asio::io_context io_context_;

  std::vector<std::shared_ptr<PeerClient>> active_workers_;

  std::vector<bool> piece_checklist_;
  uint32_t total_pieces_;
  uint32_t pieces_downloaded_;

  bool is_download_complete() const;
  int get_next_missing_piece(const PeerClient &worker) const;
  void maintain_swarm_strength();
};
