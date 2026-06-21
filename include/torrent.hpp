#pragma once

#include <string>

class TorrentFile {
public:
  std::string announce_url;
  std::string info_hash;
  long long total_length;
  long long piece_length;
  std::string master_pieces_string;
  std::string name;
  
  TorrentFile(const std::string& filepath);

  std::string get_hash_for_piece(uint32_t piece_index) const;
};
