#include "file_manager.hpp"
#include "logger.hpp"
#include <cstdint>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

FileManager::FileManager(const std::string &filepath) : filepath_(filepath) {
  std::ofstream init(filepath_, std::ios::binary | std::ios::app);
  init.close();

  file_.open(filepath_, std::ios::binary | std::ios::in | std::ios::out);
  if (!file_.is_open()) {
    throw std::runtime_error("File couldn't be opened for writing.");
  }
}

FileManager::~FileManager() {
  if (file_.is_open()) {
    file_.close();
  }
}

void FileManager::write_piece(uint32_t piece_index, uint32_t piece_length,
                              const std::vector<uint8_t> &data) {

  std::lock_guard<std::mutex> lock(file_mutex_);

  Logger::debug("Disk locked for writing Piece " + std::to_string(piece_index));

  file_.seekp(piece_index * piece_length);

  file_.write(reinterpret_cast<const char *>(data.data()), data.size());

  file_.flush();

  Logger::debug("Disk write complete. Unlocking.");
}
