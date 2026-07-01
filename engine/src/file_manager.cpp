#include "file_manager.hpp"
#include "logger.hpp"
#include "torrent.hpp"
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

FileManager::FileManager(const std::vector<FileEntry> &files) : files_(files) {
  for (const auto &file : files) {
    std::filesystem::path p(file.path);

    if (p.has_parent_path()) {
      std::filesystem::create_directories(p.parent_path());
    }

    std::ofstream init(file.path, std::ios::binary | std::ios::app);
    init.close();
  }
}

void FileManager::write_piece(uint32_t piece_index, uint32_t piece_length,
                              const std::vector<uint8_t> &data) {

  std::lock_guard<std::mutex> lock(file_mutex_);

  Logger::debug("Disk locked for writing Piece " + std::to_string(piece_index));

  uint64_t global_piece_start =
      static_cast<uint64_t>(piece_index) * piece_length;
  uint64_t global_piece_end = global_piece_start + data.size();

  for (const auto &file : files_) {
    uint64_t file_start = file.offset;
    uint64_t file_end = file_start + file.length;

    if (file_start >= global_piece_end) {
      break;
    }

    if (global_piece_start < file_end && global_piece_end > file_start) {
      uint64_t overlap_start = std::max(global_piece_start, file_start);
      uint64_t overlap_end = std::min(global_piece_end, file_end);
      uint64_t write_len = overlap_end - overlap_start;

      uint64_t buffer_offset = overlap_start - global_piece_start;

      uint64_t file_offset = overlap_start - file_start;

      std::fstream fs(file.path,
                      std::ios::binary | std::ios::in | std::ios::out);
      if (fs.is_open()) {
        fs.seekp(file_offset);
        fs.write(reinterpret_cast<const char *>(data.data() + buffer_offset),
                 write_len);
        if (fs.bad() || fs.fail()) {
          fs.close();
          throw std::runtime_error("Hardware level write failure on file: " +
                                   file.path);
        }
        fs.close();
      } else {
        throw std::runtime_error("OS denied access to open file for writing: " +
                                 file.path);
      }
    }
  }
  Logger::debug("Disk write complete. Unlocking.");
}
