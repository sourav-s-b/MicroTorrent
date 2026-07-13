#pragma once

#include "torrent.hpp"
#include <cstdint>
#include <mutex>
#include <vector>

class FileManager {
public:
  FileManager() = default;
  FileManager(const std::vector<FileEntry> &files);
  void init(const std::vector<FileEntry> &files);
  ~FileManager() = default;

  void write_piece(uint32_t piece_index, uint32_t piece_length,
                   const std::vector<uint8_t> &data);

private:
  std::vector<FileEntry> files_;
  std::mutex file_mutex_;
};
