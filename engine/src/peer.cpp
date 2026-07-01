#include "peer.hpp"
#include "asio/buffer.hpp"
#include "asio/error_code.hpp"
#include "asio/impl/read.hpp"
#include "asio/impl/write.hpp"
#include "logger.hpp"
#include "swarm.hpp"
#include <cstdint>
#include <string>
#include <vector>

PeerClient::PeerClient(asio::io_context &io_context, const std::string &ip,
                       uint16_t port, const std::string &info_hash,
                       const std::string &peer_id, SwarmManager &manager)
    : socket_(io_context), ip_(ip), info_hash_(info_hash), peer_id_(peer_id),
      manager_(manager),
      target_endpoint_(asio::ip::address::from_string(ip), port) {
  handshake_buffer_.resize(68);
}

void PeerClient::start() {
  Logger::debug("[Peer " + ip_ + "] Initialising Connection");

  socket_.async_connect(target_endpoint_, [self = shared_from_this()](
                                              const asio::error_code &ec) {
    self->handle_connect(ec);
  });
}

// the intial helpers

void PeerClient::handle_connect(const asio::error_code &ec) {
  if (ec) {
    Logger::debug("[Peer " + ip_ + "] Connection failed: " + ec.message());
    disconnect();
    return;
  }

  Logger::debug("[Peer " + ip_ + "] Socket Open. Writing Handshake...");

  std::vector<uint8_t> handshake = build_handshake();

  asio::async_write(
      socket_, asio::buffer(handshake),
      [self = shared_from_this()](const asio::error_code &ec, size_t) {
        self->handle_handshake_written(ec);
      });
}

// the actual helpers

void PeerClient::handle_handshake_written(const asio::error_code &ec) {
  if (ec) {
    disconnect();
    return;
  }

  asio::async_read(
      socket_, asio::buffer(handshake_buffer_, 68),
      [self = shared_from_this()](const asio::error_code &ec, size_t bytes) {
        self->handle_handshake_read(ec, bytes);
      });
}

void PeerClient::handle_handshake_read(const asio::error_code &ec,
                                       size_t bytes_transferred) {
  if (ec || bytes_transferred != 68) {
    Logger::debug("[Peer " + ip_ + "] Handshake failed.");
    disconnect();
    return;
  }

  std::string returned_hash(handshake_buffer_.begin() + 28,
                            handshake_buffer_.begin() + 48);

  if (returned_hash != info_hash_) {
    Logger::error("[Peer " + ip_ + "] Mismatched info_hash. Dropping.");
    disconnect();
    return;
  }

  Logger::info("[Peer " + ip_ +
                "] Handshake validated! Entering infinite event loop.");

  // and so it begins.
  read_message_header();
}

// the loopers
void PeerClient::read_message_header() {
  asio::async_read(
      socket_, asio::buffer(header_buffer_, 4),
      [self = shared_from_this()](const asio::error_code &ec, size_t) {
        self->handle_message_header(ec);
      });
}

void PeerClient::handle_message_header(const asio::error_code &ec) {
  if (ec) {
    Logger::debug("[Peer " + ip_ + "] Socket closed or network error.");
    disconnect();
    return;
  }

  uint32_t message_length = (header_buffer_[0] << 24) |
                            (header_buffer_[1] << 16) |
                            (header_buffer_[2] << 8) | header_buffer_[3];

  if (message_length == 0) { // keep-alive
    read_message_header();   // just keep listening
    return;
  }

  // messages may have different sizes
  payload_buffer_.resize(message_length);
  asio::async_read(
      socket_, asio::buffer(payload_buffer_),
      [self = shared_from_this()](const asio::error_code &ec, size_t) {
        self->handle_message_payload(ec);
      });
}

void PeerClient::handle_message_payload(const asio::error_code &ec) {
  if (ec) {
    disconnect();
    return;
  }

  uint8_t message_id = payload_buffer_[0];

  switch (message_id) {
  case 0: // choke
    Logger::debug("[Peer " + ip_ + "] Recieved Choke.");
    if (is_busy_) {
      is_busy_ = false;
      manager_.requeue_piece(active_piece_);
      active_piece_ = -1;
    }
    break;
  case 1: // unchoke
    if (!is_busy_) {
      Logger::debug("[Peer " + ip_ + "] Recieved Unchoke.");
      manager_.request_work(shared_from_this());
    }
    break;

  case 5: { // bitfield
    peer_bitfield_.clear();
    for (size_t i = 1; i < payload_buffer_.size(); ++i) {
      uint8_t current_byte = payload_buffer_[i];
      for (int bit = 7; bit >= 0; --bit) {
        peer_bitfield_.push_back((current_byte >> bit) & 1);
      }
    }
    Logger::debug("[Peer " + ip_ + "] Bitfield Received.");

    send_interested();
    break;
  }
  case 7: { // PIECE DATA
    Logger::debug("[Peer " + ip_ + "] Received Piece Data");

    if (payload_buffer_.size() <= 9)
      break;

    uint32_t msg_piece_index = (payload_buffer_[1] << 24) |
                               (payload_buffer_[2] << 16) |
                               (payload_buffer_[3] << 8) | payload_buffer_[4];

    if (msg_piece_index != active_piece_) {
      break;
    }

    uint32_t block_offset = (payload_buffer_[5] << 24) |
                            (payload_buffer_[6] << 16) |
                            (payload_buffer_[7] << 8) | payload_buffer_[8];

    uint32_t block_length = payload_buffer_.size() - 9;

    if (block_offset + block_length <= current_piece_buffer_.size()) {
      std::copy(payload_buffer_.begin() + 9, payload_buffer_.end(),
                current_piece_buffer_.begin() + block_offset);
      bytes_downloaded_ += block_length;
    }

    if (bytes_downloaded_ >= current_piece_length_) {
      is_busy_ = false;
      active_piece_ = -1;
      Logger::debug("[Peer " + ip_ + "] Piece " +
                    std::to_string(current_piece_index_) +
                    " fully downloaded!");
      manager_.submit_piece(shared_from_this(), current_piece_index_,
                            current_piece_buffer_);
    } else {
      request_next_block();
    }
    break;
  }
  }

  read_message_header(); // loop back
}

// the actual helpers

std::vector<uint8_t> PeerClient::build_handshake() {
  std::vector<uint8_t> handshake;

  handshake.push_back(19);
  std::string pstr = "BitTorrent protocol"; // length of protocol string

  for (char c : pstr) {
    handshake.push_back(static_cast<uint8_t>(c)); // the actual protocol string
  }
  for (int i = 0; i < 8; ++i) {
    handshake.push_back(0x00); // reserved zeroes
  }

  for (char c : info_hash_) {
    handshake.push_back(static_cast<uint8_t>(c)); // info hash
  }
  for (char c : peer_id_) {
    handshake.push_back(static_cast<uint8_t>(c)); // peer id
  }
  return handshake;
}

void PeerClient::send_interested() {
  auto msg = std::make_shared<std::vector<uint8_t>>(
      std::vector<uint8_t>{0, 0, 0, 1, 2});
  asio::async_write(
      socket_, asio::buffer(*msg),
      [self = shared_from_this(), msg](const asio::error_code &ec, size_t) {
        if (!ec)
          Logger::debug("[Peer " + self->ip_ + "] Sent INTERESTED.");
      });
}

void PeerClient::push_uint32(std::vector<uint8_t> &vec, uint32_t val) {
  vec.push_back((val >> 24) & 0xFF);
  vec.push_back((val >> 16) & 0xFF);
  vec.push_back((val >> 8) & 0xFF);
  vec.push_back(val & 0xFF);
}

void PeerClient::request_block(uint32_t piece_index, uint32_t block_offset,
                               uint32_t block_length) {
  std::vector<uint8_t> request_msg;

  push_uint32(request_msg, 13);

  request_msg.push_back(6);

  push_uint32(request_msg, piece_index);
  push_uint32(request_msg, block_offset);
  push_uint32(request_msg, block_length);

  asio::write(socket_, asio::buffer(request_msg));
  Logger::debug("Sent: REQUEST for Piece " + std::to_string(piece_index) +
                " at offset " + std::to_string(block_offset) + " (" +
                std::to_string(block_length) + " bytes)");
}

bool PeerClient::has_piece(uint32_t piece_index) const {
  if (piece_index >= peer_bitfield_.size()) {
    Logger::error(std::to_string(piece_index) +
                  ": piece index asked from peer is out of bound");
    return false;
  }
  return peer_bitfield_[piece_index];
}

// state tracking
void PeerClient::fetch_piece_async(uint32_t piece_index,
                                   uint32_t piece_length) {
  current_piece_index_ = piece_index;
  current_piece_length_ = piece_length;
  bytes_downloaded_ = 0;
  is_busy_ = true;
  active_piece_ = piece_index;

  current_piece_buffer_.clear();
  current_piece_buffer_.assign(piece_length, 0);

  Logger::debug("[Peer " + ip_ + "] Starting async fetch for Piece " +
                std::to_string(piece_index));

  request_next_block();
}

void PeerClient::request_next_block() {
  if (bytes_downloaded_ >= current_piece_length_)
    return;

  const uint32_t MAX_BLOCK_LENGTH = 16 * 1024;
  uint32_t current_block_size =
      std::min(MAX_BLOCK_LENGTH,
               current_piece_length_ - bytes_downloaded_); // for last piece

  request_block(current_piece_index_, bytes_downloaded_, current_block_size);
}

void PeerClient::disconnect() {
  asio::error_code ec;
  if (socket_.is_open()) {
    ec = socket_.close(ec);
  }
  Logger::info("[Peer " + ip_ + "] Disconnected manually.");

  manager_.handle_disconnect(shared_from_this());
}
