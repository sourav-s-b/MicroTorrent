#pragma once

#include "peer.hpp"
#include "torrent.hpp"
#include <cstdint>
#include <string>
#include <vector>
struct PeerData {
  std::string ip;
  uint16_t port;
};

class SwarmManager {
public:
  SwarmManager(const TorrentFile &torrent, const std::vector<PeerData> &peers);

  void start_download();

private:
  TorrentFile torrent_;
  std::vector<PeerData> peers_;

  std::vector<bool> piece_checklist_;

  uint32_t total_pieces_;
  uint32_t pieces_downloaded_;

  bool is_download_complete() const;
  int get_next_missing_piece(const PeerClient& worker) const;
};
