#include "peer.hpp"
#include "asio/error_code.hpp"
#include "logger.hpp"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

PeerClient::PeerClient(asio::io_context &io_context, const std::string &ip,
                       uint16_t port, const std::string &info_hash,
                       const std::string &peer_id)
    : socket_(io_context), ip_(ip), port_(port), info_hash_(info_hash),
      peer_id_(peer_id) {}

std::vector<uint8_t> PeerClient::build_handshake() {
  std::vector<uint8_t> handshake;

  handshake.push_back(19);

  std::string pstr = "BitTorrent protocol";
  for (char c : pstr) {
    handshake.push_back(static_cast<uint8_t>(c));
  }

  for (int i = 0; i < 8; ++i) {
    handshake.push_back(0x00);
  }

  for (char c : info_hash_) {
    handshake.push_back(static_cast<uint8_t>(c));
  }

  for (char c : peer_id_) {
    handshake.push_back(static_cast<uint8_t>(c));
  }

  return handshake;
}

bool PeerClient::connect_and_handshake() {
  try {
    Logger::info("[Peer " + ip_ + ":" + std::to_string(port_) +
                 "] Connecting...");

    asio::ip::tcp::endpoint target(asio::ip::address::from_string(ip_), port_);
    asio::error_code ec;

    ec = socket_.connect(target, ec);
    if (ec) {
      Logger::error("Connection failed: " + ec.message());
      return false;
    }

    Logger::info("Socket Open. Sending Handshake...");

    std::vector<uint8_t> handshake = build_handshake();
    asio::write(socket_, asio::buffer(handshake));

    std::vector<uint8_t> peer_response(68);

    asio::read(socket_, asio::buffer(peer_response), ec);
    if (ec) {
      Logger::error("Handshake failed: " + ec.message());
    }

    std::string returned_hash(peer_response.begin() + 28,
                              peer_response.begin() + 48);
    if (returned_hash != info_hash_) {
      Logger::error("SECURITY ALERT: Peer returned mismatched info_hash : "
                    "Dropping connection.");
      socket_.close();
      return false;
    }

    Logger::info("Success! Handshake validated. Securely Linekd");

    int msg1 = receive_message(); // for bitfield
    if (msg1 != 5) {
      Logger::error(
          "Peer didn't send a BITFIELD. They have 0 pieces : Dropping them.");
      return false;
    }

    send_interested();

    int msg2 = receive_message(); // for unchoke
    if (msg2 != 1) {
      Logger::error("Peer refused to UNCHOKE us. Dropping them.");
      return false;
    }
    return true;

  } catch (std::exception &e) {
    return false;
  }
}

void PeerClient::send_interested() {
  std::vector<uint8_t> msg = {0, 0, 0, 1, 2};
  asio::write(socket_, asio::buffer(msg));
  Logger::debug("Sent: Interested");
}

int PeerClient::receive_message() {
  asio::error_code ec;
  uint8_t length_buffer[4];

  asio::read(socket_, asio::buffer(length_buffer, 4), ec);
  if (ec) {
    Logger::error("Socket closed while waiting for message length.");
    return false;
  }

  uint32_t message_length = (length_buffer[0] << 24) |
                            (length_buffer[1] << 16) | (length_buffer[2] << 8) |
                            length_buffer[3];

  if (message_length == 0) {
    Logger::debug("Received Keep-Alive");
    return -1;
  }

  std::vector<uint8_t> payload(message_length);
  asio::read(socket_, asio::buffer(payload), ec);
  if (ec) {
    Logger::error("Failed to read message payload.");
    return -2;
  }

  uint8_t message_id = payload[0];
  switch (message_id) {
  case 0:
    Logger::debug("Received: CHOKE (ID: 0)");
    break;
  case 1:
    Logger::debug("Received: UNCHOKE (ID: 1) - We can now request data!");
    break;
  case 5:
    Logger::debug("Received: BITFIELD (ID: 5) - Size: " +
                  std::to_string(payload.size() - 1) + " bytes");
    break;
  case 7: {
    Logger::debug("Recieved: PIECE ");

    last_data_buffer_.clear();

    last_data_buffer_.insert(last_data_buffer_.end(), payload.begin() + 9,
                             payload.end());
    break;
  }
  default:
    Logger::debug("Received: Message ID " + std::to_string(message_id));
    break;
  }
  return message_id;
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

std::vector<uint8_t> PeerClient::download_piece(uint32_t piece_index,
                                                uint32_t piece_length) {
  std::vector<uint8_t> piece_buffer;
  piece_buffer.reserve(piece_length);

  uint32_t bytes_downloaded = 0;
  const uint32_t MAX_BLOCK_LENGTH = 16 * 1024; // 16KB

  while (bytes_downloaded < piece_length) {

    uint32_t current_block_size =
        std::min(MAX_BLOCK_LENGTH, piece_length - bytes_downloaded);

    request_block(piece_index, bytes_downloaded, current_block_size);

    int msg_id;
    while (true) {
      msg_id = receive_message();

      if (msg_id == 7) {
        break;
      } else if (msg_id < 0) {
        Logger::error("Error: Peer disconnected or socket failed.");
        return {};
      } else {
        Logger::debug("Ignoring protocol chatter (ID: " +
                      std::to_string(msg_id) + ")...");
      }
    }

    bytes_downloaded += current_block_size;
    piece_buffer.insert(piece_buffer.end(), last_data_buffer_.begin(),
                        last_data_buffer_.end());

    Logger::progress("Progress: " + std::to_string(bytes_downloaded) + " / " +
                     std::to_string(piece_length) + " bytes", LogLevel::INFO);
  }
  Logger::info("PIECE " + std::to_string(piece_index) + " DOWNLOAD COMPLETE!");
  return piece_buffer;
}

bool PeerClient::has_piece(uint32_t piece_index) const {
    if (piece_index >= peer_bitfield_.size()) {
        Logger::error(std::to_string(piece_index) + ": piece index asked from peer is out of bound");
        return false;
    }
    return peer_bitfield_[piece_index];
}