#include "torrent.hpp"
#include "bencode.hpp"
#include "logger.hpp"
#include "utils.hpp"
#include <stdexcept>
#include <string>

TorrentFile::TorrentFile(const std::string &filepath) {
  Logger::info("Loading torrent file " + filepath);

  std::string file_buffer = read_binary_file(filepath);

  size_t index = 0;
  BencodeNode root = parse_bencode(file_buffer, index);
  BencodeDict torrent_info = std::get<BencodeDict>(root.data);

  announce_url =
      std::get<std::string>(torrent_info.at("announce").data);

  BencodeNode info_node = torrent_info.at("info");
  BencodeDict info = std::get<BencodeDict>(info_node.data);

  piece_length = std::get<long long>(info.at("piece length").data);
  master_pieces_string = std::get<std::string>(info.at("pieces").data);
  name = std::get<std::string>(info.at("name").data);

  if (info.count("length")) {
      is_folder = false;
      total_length = std::get<long long>(info.at("length").data);

      FileEntry file;
      file.path = name;
      file.length = total_length;
      file.offset = 0;
      file_list.push_back(file);
  } else if (info.count("files")) {
      is_folder = true;
      long long current_global_offset = 0;

      BencodeList files = std::get<BencodeList>(info.at("files").data);
      for (const auto& file: files) {
          BencodeDict file_dict = std::get<BencodeDict>(file.data);


          std::string full_path = name;
          BencodeList path_list = std::get<BencodeList>(file_dict.at("path").data);
          for (const auto& path : path_list) {
               full_path += "/" + std::get<std::string>(path.data);
          }


          FileEntry file_entry;
          file_entry.path = full_path;
          file_entry.length = std::get<long long>(file_dict.at("length").data);
          file_entry.offset = current_global_offset;
          file_list.push_back(file_entry);

          current_global_offset += file_entry.length;
      }
      total_length = current_global_offset;
  } else {
      throw std::runtime_error("Given Torrent has neither the field length nor files");
  }

  std::string info_slice = encode_bencode(info_node);
  info_hash = SHA1::hash(info_slice);

  Logger::info("Torrent Parsed Successfully.");
  Logger::info("Total Size: " + std::to_string(total_length) + " bytes");
  Logger::info("File Count: " + std::to_string(file_list.size()));
  Logger::info("Piece Size: " + std::to_string(piece_length) + " bytes");
}

std::string TorrentFile::get_hash_for_piece(uint32_t piece_index) const {
    size_t start = piece_index * 20;

    if (start + 20 > master_pieces_string.length()) {
        throw std::out_of_range("Requested piece index is out of bounds.");
    }

    return master_pieces_string.substr(start, 20);
}
