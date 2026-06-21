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

class SwarmManager {
public:
  SwarmManager(const TorrentFile &torrent, const std::vector<PeerData> &peers);

  void start_download();

  void request_work(std::shared_ptr<PeerClient> worker);

  void submit_piece(std::shared_ptr<PeerClient> worker, uint32_t piece_index,
                    const std::vector<uint8_t> &data);

  void handle_disconnect(std::shared_ptr<PeerClient> worker);

private:
  TorrentFile torrent_;
  std::vector<PeerData> peers_;
  FileManager disk_;

  asio::io_context io_context_;

  std::vector<std::shared_ptr<PeerClient>> active_workers_;

  std::vector<bool> piece_checklist_;
  uint32_t total_pieces_;
  uint32_t pieces_downloaded_;

  bool is_download_complete() const;
  int get_next_missing_piece(const PeerClient &worker) const;
};
