#include "peer.hpp"
#include "asio/error_code.hpp"
#include <cstdint>
#include <exception>
#include <fstream>
#include <iostream>
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
    request_block(0, 0, 16384);
    receive_message();

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

    uint32_t piece_index = (payload[1] << 24) | (payload[2] << 16) |
                           (payload[3] << 8) | payload[4];
    uint32_t block_offset = (payload[5] << 24) | (payload[6] << 16) |
                            (payload[7] << 8) | payload[8];

    size_t data_size = payload.size() - 9;

    std::cout << "   -> Index: " << piece_index
              << " | Offeset: " << block_offset
              << " | Block size: " << data_size << " bytes" << std::endl;

    std::ofstream out_file("debian_output_file.iso",
                           std::ios::binary | std::ios::app);
    if (out_file.is_open()) {
      out_file.write(reinterpret_cast<const char *>(payload.data() + 9),
                     data_size);
      out_file.close();
      std::cout << "     -> SUCCESS! Wrote data to disk!" << std::endl;
    } else {
      std::cerr << "     -> FAILED to open file for writing." << std::endl;
    }
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
