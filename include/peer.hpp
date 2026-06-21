#pragma once

#include <asio.hpp>
#include <cstdint>
#include <string>
#include <vector>

class PeerClient {
public:
  PeerClient(asio::io_context &io_context, const std::string &ip, uint16_t port,
             const std::string &info_hash, const std::string &peer_id);

  bool connect_and_handshake();
  void request_block(uint32_t piece_index, uint32_t block_offset,
                     uint32_t block_length);
  std::vector<uint8_t> download_piece(uint32_t piece_index,
                                      uint32_t piece_length);
  bool has_piece(uint32_t piece_index) const;

private:
  asio::ip::tcp::socket socket_;
  std::string ip_;
  uint16_t port_;
  std::string info_hash_;
  std::string peer_id_;
  std::vector<uint8_t> last_data_buffer_;
  std::vector<bool> peer_bitfield_;

  std::vector<uint8_t> build_handshake();
  int receive_message();
  void send_interested();
  void push_uint32(std::vector<uint8_t> &vec, uint32_t val);
};
