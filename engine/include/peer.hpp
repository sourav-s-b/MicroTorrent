#pragma once

#include "asio/error_code.hpp"
#include <asio.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class SwarmManager;

class PeerClient : public std::enable_shared_from_this<PeerClient> {
public:
  PeerClient(asio::io_context &io_context, const std::string &ip, uint16_t port,
             const std::string &info_hash, const std::string &peer_id,
             SwarmManager &manager);

  int get_active_piece() const { return active_piece_; }
  std::string get_ip() const { return ip_; }

  void start();
  bool has_piece(uint32_t piece_index) const;
  void fetch_piece_async(uint32_t piece_index, uint32_t piece_length);
  bool is_actively_downloading() const;
  void disconnect();

private:
  asio::ip::tcp::socket socket_;
  asio::ip::tcp::endpoint target_endpoint_;
  std::string ip_;
  std::string info_hash_;
  std::string peer_id_;
  SwarmManager &manager_;

  std::vector<uint8_t> handshake_buffer_;
  uint8_t header_buffer_[4];
  std::vector<uint8_t> payload_buffer_;
  std::vector<bool> peer_bitfield_;
  bool peer_interested_ = false;

  // state tracking
  uint32_t current_piece_index_;
  uint32_t current_piece_length_;
  uint32_t bytes_downloaded_;
  std::vector<uint8_t> current_piece_buffer_;
  bool is_busy_ = false;
  int active_piece_ = -1;
  uint32_t bytes_requested_ = 0;

  // the initial helpers
  void handle_connect(const asio::error_code &ec);
  void handle_handshake_written(const asio::error_code &ec);
  void handle_handshake_read(const asio::error_code &ec,
                             size_t bytes_transferred);

  // the loopers
  void read_message_header();
  void handle_message_header(const asio::error_code &ec);
  void handle_message_payload(const asio::error_code &ec);

  // actual helpers
  std::vector<uint8_t> build_handshake();
  void send_interested();
  void request_block(uint32_t piece_index, uint32_t block_offset,
                     uint32_t block_length);
  void push_uint32(std::vector<uint8_t> &vec, uint32_t val);
  void request_next_block();

  const uint32_t MAX_BLOCK_WINDOW = 7;
};
