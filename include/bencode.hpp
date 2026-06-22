#pragma once

#include <variant>
#include <map>
#include <string>
#include <vector>

struct BencodeNode;

using BencodeList = std::vector<BencodeNode>;
using BencodeDict = std::map<std::string, BencodeNode>;

struct BencodeNode {
    std::variant<long long , std::string , BencodeList, BencodeDict> data;
};

BencodeNode parse_bencode(const std::string& buffer, size_t& index);

std::string encode_bencode(const BencodeNode& node);
