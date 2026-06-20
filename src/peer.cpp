#include "peer.hpp"
#include "asio/error_code.hpp"
#include <algorithm>
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
#include <stdexcept>
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
    std::cout << "\n[Peer " << ip_ << ":" << port_ << "] Connecting..."
              << std::endl;

    asio::ip::tcp::endpoint target(asio::ip::address::from_string(ip_), port_);
    asio::error_code ec;

    ec = socket_.connect(target, ec);
    if (ec) {
      std::cerr << " -> Connection failed: " << ec.message() << std::endl;
      return false;
    }

    std::cout << " -> Socket Open. Sending Handshake..." << std::endl;

    std::vector<uint8_t> handshake = build_handshake();
    asio::write(socket_, asio::buffer(handshake));

    std::vector<uint8_t> peer_response(68);

    asio::read(socket_, asio::buffer(peer_response), ec);
    if (ec) {
      std::cerr << " -> Handshake failed: " << ec.message() << std::endl;
    }

    std::string returned_hash(peer_response.begin() + 28,
                              peer_response.begin() + 48);
    if (returned_hash != info_hash_) {
      std::cerr << " -> SECURITY ALERT: Peer returned mismatched info_hash. "
                   "Dropping connection."
                << std::endl;
      socket_.close();
      return false;
    }

    std::cout << " -> Success!\nHandshake validated. Securely Linekd"
              << std::endl;

    int msg1 = receive_message(); // for bitfield
    if (msg1 != 5) {
      std::cout << "  -> Peer didn't send a BITFIELD. They have 0 pieces. "
                   "Dropping them."
                << std::endl;
      return false;
    }

    send_interested();

    int msg2 = receive_message(); // for unchoke
    if (msg2 != 1) {
      std::cout << "  -> Peer refused to UNCHOKE us. Dropping them."
                << std::endl;
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
  std::cout << " -> Sent: Interested" << std::endl;
}

int PeerClient::receive_message() {
  asio::error_code ec;
  uint8_t length_buffer[4];

  asio::read(socket_, asio::buffer(length_buffer, 4), ec);
  if (ec) {
    std::cerr << " -> Socket closed while waiting for message length."
              << std::endl;
    return false;
  }

  uint32_t message_length = (length_buffer[0] << 24) |
                            (length_buffer[1] << 16) | (length_buffer[2] << 8) |
                            length_buffer[3];

  if (message_length == 0) {
    std::cout << " <- Received Keep-Alive" << std::endl;
    return -1;
  }

  std::vector<uint8_t> payload(message_length);
  asio::read(socket_, asio::buffer(payload), ec);
  if (ec) {
    std::cerr << "  -> Failed to read message payload." << std::endl;
    return -2;
  }

  uint8_t message_id = payload[0];
  switch (message_id) {
  case 0:
    std::cout << "  <- Received: CHOKE (ID: 0)" << std::endl;
    break;
  case 1:
    std::cout << "  <- Received: UNCHOKE (ID: 1) - We can now request data!"
              << std::endl;
    break;
  case 5:
    std::cout << "  <- Received: BITFIELD (ID: 5) - Size: "
              << payload.size() - 1 << " bytes" << std::endl;
    break;
  case 7: {
    std::cout << "\n Recieved: PIECE " << std::endl;

    last_data_buffer_.clear();

    last_data_buffer_.insert(last_data_buffer_.end(), payload.begin() + 9,
                             payload.end());
    break;
  }
  default:
    std::cout << "  <- Received: Message ID " << (int)message_id << std::endl;
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
  std::cout << "  -> Sent: REQUEST for Piece " << piece_index << " at offset "
            << block_offset << " (" << block_length << " bytes)" << std::endl;
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
                } 
                else if (msg_id < 0) {
                    std::cerr << "  -> Error: Peer disconnected or socket failed." << std::endl;
                    return {}; 
                } 
                else {
                    std::cout << "     -> Ignoring protocol chatter (ID: " << msg_id << ")..." << std::endl;
                }
            }

    bytes_downloaded += current_block_size;
    piece_buffer.insert(piece_buffer.end(), last_data_buffer_.begin(),
                        last_data_buffer_.end());

    std::cout << "     -> Progress: " << bytes_downloaded << " / "
              << piece_length << " bytes" << std::endl;
  }
  std::cout << "--- PIECE " << piece_index << " DOWNLOAD COMPLETE! ---"
            << std::endl;
  return piece_buffer;
}
