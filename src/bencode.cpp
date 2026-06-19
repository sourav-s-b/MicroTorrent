#include "bencode.hpp"
#include <cctype>
#include <stdexcept>


BencodeNode parse_list(const std::string& buffer , size_t& index);
BencodeNode parse_string(const std::string& buffer , size_t& index);
BencodeNode parse_integer(const std::string& buffer , size_t& index);
BencodeNode parse_dict(const std::string& buffer , size_t& index);



BencodeNode parse_bencode(const std::string& buffer , size_t& index) {
    if (index >= buffer.size()) {
        throw std::runtime_error("Unexpected end of buffer while parsing Bencode");
    }

    char current_byte = buffer[index];

    if (std::isdigit(current_byte)) {
        return parse_string(buffer , index);
    } else if (current_byte == 'i') {
        return parse_integer(buffer, index);
    } else if (current_byte == 'l') {
        return parse_list(buffer, index);
    }   else if (current_byte == 'd') {
        return parse_dict(buffer, index);
    }

    throw std::runtime_error(std::string("Unkown or unhandled Bencode type character found: ") + current_byte + " at index: " + std::to_string(index) );
}

BencodeNode parse_integer(const std::string& buffer ,size_t& index) {
   index++;

  size_t end_pos = buffer.find('e' , index);

  if (end_pos == std::string::npos) {
      throw std::runtime_error("Invalid Bencode integer: no closing 'e' found.");
  }
  std::string num_str = buffer.substr(index , end_pos - index);
  long long value = std::stoll(num_str);

  index = end_pos + 1;

  BencodeNode node;
  node.data = value;
  return node;
}

BencodeNode parse_string(const std::string& buffer ,size_t& index) {

    size_t colon_pos = buffer.find(':' , index);

    if (colon_pos == std::string::npos) {
          throw std::runtime_error(std::string("Invalid Bencode integer: no ':' found.") + " at index: " + std::to_string(index)) ;
    }

    std::string length_str = buffer.substr(index, colon_pos-index);
    long long length = std::stoll(length_str);

    index = colon_pos + 1;

    if (index + length > buffer.size()) {
        throw std::runtime_error("Invalid Bencode string: length out of bounds");
    }
    std::string data = buffer.substr(index , length);

    index += length;

    BencodeNode node;
    node.data = data;
    return node;
}

BencodeNode parse_list(const std::string& buffer , size_t& index) {
    index++;

    BencodeList list_contents;

    while (index < buffer.size() && buffer[index] != 'e') {
        BencodeNode element = parse_bencode(buffer , index);
        list_contents.push_back(element);
    }

    if (index >= buffer.size()) {
        throw std::runtime_error("Invalid Bencode list: missing closing 'e' marker.");
    }

    index++;
    BencodeNode node;
    node.data = list_contents;
    return node;
}


BencodeNode parse_dict(const std::string& buffer , size_t& index) {
    index++;

    BencodeDict dict_content;
    while (index < buffer.size() && buffer[index] != 'e'){
        if (!std::isdigit(buffer[index])){
            throw std::runtime_error(std::string("Invalid Bencode dictionary: keys must be strings.")+ " at index: " + std::to_string(index));
        }

        BencodeNode key_node = parse_string(buffer, index);
        std::string key = std::get<std::string>(key_node.data);

        BencodeNode value = parse_bencode(buffer, index);
        dict_content[key] = value;
    }

    if (index >= buffer.size()) {
        throw std::runtime_error("Invalid Bencode dictionary: missing closing 'e' marker.");
    }

    index++;

    BencodeNode node;
    node.data = dict_content;
    return node;
}
