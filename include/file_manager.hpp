#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

class FileManager {
public:
  FileManager(const std::string& filepath);
  ~FileManager();

  void write_piece(uint32_t piece_index, uint32_t piece_length, const std::vector<uint8_t>& data);

private:
    std::string filepath_;
    std::fstream file_;
    std::mutex file_mutex_;
};