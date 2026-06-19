#pragma once

#include <fstream>
#include <iomanip>
#include <sstream>
#include <string>

class SHA1 {

public:
  static std::string hash(const std::string &input) {
    uint32_t h0 = 0x67452301, h1 = 0xEFCDAB89, h2 = 0x98BADCFE, h3 = 0x10325476,
             h4 = 0xC3D2E1F0;

    std::string data = input;
    uint64_t original_bit_len = data.length() * 8;
    data += (char)0x80;

    while ((data.length() * 8) % 512 != 448) {
      data += (char)0x00;
    }

    for (int i = 7; i >= 0; --i) {
      data += (char)((original_bit_len >> (i * 8)) & 0xFF);
    }

    for (size_t offset = 0; offset < data.length(); offset += 64) {
      uint32_t w[80];
      for (int i = 0; i < 16; ++i) {
        w[i] = ((unsigned char)data[offset + i * 4] << 24) |
               ((unsigned char)data[offset + i * 4 + 1] << 16) |
               ((unsigned char)data[offset + i * 4 + 2] << 8) |
               ((unsigned char)data[offset + i * 4 + 3]);
      }
      for (int i = 16; i < 80; ++i) {
        uint32_t temp = w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16];
        w[i] = (temp << 1) | (temp >> 31); // Left rotate by 1
      }

      uint32_t a = h0, b = h1, c = h2, d = h3, e = h4;

      for (int i = 0; i < 80; ++i) {
        uint32_t f, k;
        if (i < 20) {
          f = (b & c) | ((~b) & d);
          k = 0x5A827999;
        } else if (i < 40) {
          f = b ^ c ^ d;
          k = 0x6ED9EBA1;
        } else if (i < 60) {
          f = (b & c) | (b & d) | (c & d);
          k = 0x8F1BBCDC;
        } else {
          f = b ^ c ^ d;
          k = 0xCA62C1D6;
        }

        uint32_t temp = ((a << 5) | (a >> 27)) + f + e + k + w[i];
        e = d;
        d = c;
        c = (b << 30) | (b >> 2);
        b = a;
        a = temp;
      }

      h0 += a;
      h1 += b;
      h2 += c;
      h3 += d;
      h4 += e;
    }

    // Combine the 5 integers back into exactly 20 raw bytes
    std::string raw_hash(20, '\0');
    uint32_t hashes[5] = {h0, h1, h2, h3, h4};
    for (int i = 0; i < 5; ++i) {
      raw_hash[i * 4] = (hashes[i] >> 24) & 0xFF;
      raw_hash[i * 4 + 1] = (hashes[i] >> 16) & 0xFF;
      raw_hash[i * 4 + 2] = (hashes[i] >> 8) & 0xFF;
      raw_hash[i * 4 + 3] = hashes[i] & 0xFF;
    }

    return raw_hash;
  }
};

inline std::string url_encode(const std::string &value) {
  std::ostringstream escaped;
  escaped << std::hex << std::uppercase;

  for (unsigned char c : value) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      escaped << c;
    } else {
      escaped << '%' << std::setw(2) << std::setfill('0') << (int)c;
    }
  }

  return escaped.str();
}

inline std::string read_binary_file(const std::string &file_path) {
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    throw std::runtime_error("Could not open file at path: " + file_path);
  }

  std::streamsize size = file.tellg();
  file.seekg(0, std::ios::beg);

  std::string buffer(size, '\0');
  if (!file.read(&buffer[0], size)) {
    throw std::runtime_error("Failed to read complete file buffer data.");
  }
  return buffer;
}
