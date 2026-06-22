#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct FileEntry {
  std::string path;
  long long length;
  long long offset;
};

class TorrentFile {
public:
  TorrentFile(const std::string &filepath);
  std::string get_hash_for_piece(uint32_t piece_index) const;

  std::string announce_url;
  std::string info_hash;
  long long total_length;
  long long piece_length;
  std::string master_pieces_string;
  std::string name;

  bool is_folder;
  std::vector<FileEntry> file_list;
};
