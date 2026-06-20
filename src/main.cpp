#include "asio/io_context.hpp"
#include "bencode.hpp"
#include "peer.hpp"
#include "tracker.hpp"
#include "utils.hpp"
#include <iostream>
#include <stdexcept>

int main() {
  std::string torrent_path = "debian-13.5.0-amd64-netinst.iso.torrent";

  try {
    std::cout << "1. Loading binary file into memory..." << std::endl;
    std::string file_buffer = read_binary_file(torrent_path);

    std::cout << "2. Validating Bencode structure..." << std::endl;
    size_t index = 0;
    BencodeNode root = parse_bencode(
        file_buffer, index); // Let it parse to ensure it's a valid file
    BencodeDict torrent_info = std::get<BencodeDict>(root.data);

    std::cout << "3. Locating raw 'info' block for hashing..." << std::endl;
    BencodeNode info_node = torrent_info["info"];

    std::string info_slice = encode_bencode(info_node);
    std::cout << "   -> Extracted exactly " << info_slice.size() << " bytes."
              << std::endl;

    std::cout << "4. Executing Cryptographic Pipeline..." << std::endl;

    std::string hash = SHA1::hash(info_slice);

    std::string url_encoded = url_encode(hash);

    std::cout << "\nSUCCESS!" << std::endl;
    std::cout << "URL Encoded Hash: " << url_encoded << std::endl;

    std::string announce_url =
        std::get<std::string>(torrent_info["announce"].data);
    BencodeDict info_dict = std::get<BencodeDict>(torrent_info["info"].data);
    long long total_length = std::get<long long>(info_dict["length"].data);

    std::string response =
        TrackerClient::announce(announce_url, hash, total_length);

    size_t header_end = response.find("\r\n\r\n");
    if (header_end == std::string::npos) {
      throw std::runtime_error(
          "Invalid Tracker Response: Missing HTTP body delimiter.");
    }

    std::string body = response.substr(header_end + 4);

    size_t parse_index = 0;
    BencodeNode tracker_node = parse_bencode(body, parse_index);
    BencodeDict tracker_dict = std::get<BencodeDict>(tracker_node.data);

    std::string peers_binary =
        std::get<std::string>(tracker_dict["peers"].data);

    std::cout << "Successfully extracted " << peers_binary.length() / 6
              << " peers from the tracker.\n"
              << std::endl;

    std::string target_ip;
    uint16_t target_port = 0;
    asio::io_context peer_io_context;
    bool successfull_handshake = false;
    std::string peer_id = "-MT0001-174094882455";

    for (size_t i = 0; i < peers_binary.length(); i += 6) {
      uint8_t ip1 = peers_binary[i];
      uint8_t ip2 = peers_binary[i + 1];
      uint8_t ip3 = peers_binary[i + 2];
      uint8_t ip4 = peers_binary[i + 3];

      target_ip = std::to_string(ip1) + "." + std::to_string(ip2) + "." +
                  std::to_string(ip3) + "." + std::to_string(ip4);

      target_port = (static_cast<uint8_t>(peers_binary[i + 4]) << 8) |
                    static_cast<uint8_t>(peers_binary[i + 5]);

      std::cout << "Found Peer -> " << target_ip << ":" << target_port
                << std::endl;

      PeerClient worker(peer_io_context, target_ip, target_port, hash, peer_id);
      if (worker.connect_and_handshake()) {
        successfull_handshake = true;
        break;
      } else {
        std::cout << "  -> Moving to the next peer in the swarm...\n"
                  << std::endl;
      }

    }
    if (!successfull_handshake) {
        std::cout << "Couldn't Connect to any peers.." << std::endl;
        return 0;
    }

  } catch (const std::exception &e) {
    std::cerr << "Fatal Error: " << e.what() << std::endl;
  }
  return 0;
}
