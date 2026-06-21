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

  total_length = std::get<long long>(info.at("length").data);
  piece_length = std::get<long long>(info.at("piece length").data);
  master_pieces_string = std::get<std::string>(info.at("pieces").data);
  name = std::get<std::string>(info.at("name").data);

  std::string info_slice = encode_bencode(info_node);
  info_hash = SHA1::hash(info_slice);

  Logger::debug("Torrent Parsed Successfully.");
  Logger::debug("Total Size: " + std::to_string(total_length) + " bytes");
  Logger::debug("Piece Size: " + std::to_string(piece_length) + " bytes");
}

std::string TorrentFile::get_hash_for_piece(uint32_t piece_index) const {
    size_t start = piece_index * 20;

    if (start + 20 > master_pieces_string.length()) {
        throw std::out_of_range("Requested piece index is out of bounds.");
    }

    return master_pieces_string.substr(start, 20);
}